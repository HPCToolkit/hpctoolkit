// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

//************************ System Include Files ******************************

#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib> // for 'exit'
using std::exit; // For compatibility with non-std C headers
#endif

//*********************** Xerces Include Files *******************************

#include <xercesc/util/XMLString.hpp> 
using XERCES_CPP_NAMESPACE::XMLString;

#include <xercesc/parsers/XercesDOMParser.hpp>
using XERCES_CPP_NAMESPACE::XercesDOMParser;

#include <xercesc/dom/DOMNamedNodeMap.hpp> 
using XERCES_CPP_NAMESPACE::DOMNamedNodeMap;


//************************* User Include Files *******************************

#include "ConfigParser.hpp"

#include "Driver.hpp"
#include "MathMLExprParser.hpp"
#include "DerivedPerfMetrics.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>
#include <lib/prof-juicy/PerfMetric.hpp>

#include <lib/support/Trace.hpp>


//************************ Forward Declarations ******************************

#define DBG_ME 0

//****************************************************************************


// -------------------------------------------------------------------------
// declare forward references 
// -------------------------------------------------------------------------

static void ProcessDOCUMENT(DOMNode *node, Driver &driver, bool onlyMetrics);
static void ProcessHPCVIEW(DOMNode *node, Driver &driver, bool onlyMetrics);
static void ProcessELEMENT(DOMNode *node, Driver &driver, bool onlyMetrics);
static void ProcessMETRIC(DOMNode *node, Driver &driver);
static void ProcessFILE(DOMNode *fileNode, Driver &driver, 
			const string& metricName, bool metricDoDisplay, 
			bool metricDoPercent, bool metricDoSortBy, 
			const string& metricDisplayName);

static string getAttr(DOMNode *node, const XMLCh *attrName);


void 
PrintName(DOMNode *node)
{
  const XMLCh *nodeName = node->getNodeName();
  const XMLCh *nodeValue = node->getNodeValue();
  cerr << "name=" << XMLString::transcode(nodeName) 
       << " value=" << XMLString::transcode(nodeValue) << endl;
}

// -------------------------------------------------------------------------
// external operations
// -------------------------------------------------------------------------

ConfigParser::ConfigParser(const string& inputFile, 
				   XercesErrorHandler& errHndlr)
{
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << inputFile);

  mParser = new XercesDOMParser;
  mParser->setValidationScheme(XercesDOMParser::Val_Auto);
  mParser->setErrorHandler(&errHndlr);
  mParser->parse(inputFile.c_str());
  if (mParser->getErrorCount() > 0) {
    ConfigParser_Throw("terminating because of previously reported CONFIGURATION file parse errors.");
  }
  
  mDoc = mParser->getDocument();
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: "<< "document: " << mDoc);
}


ConfigParser::~ConfigParser()
{
  delete mParser;
}


void
ConfigParser::pass1(Driver& driver)
{
  ProcessDOCUMENT(mDoc, driver, false /*onlyMetrics*/);
}


void
ConfigParser::pass2(Driver& driver)
{
  // FIXME: should not really need driver
  ProcessDOCUMENT(mDoc, driver, true /*onlyMetrics*/);
}


// -------------------------------------------------------------------------
// internal operations
// -------------------------------------------------------------------------

static void 
ProcessDOCUMENT(DOMNode *node, Driver &driver, bool onlyMetrics)
{
  DOMNode *child = node->getFirstChild();
  ProcessHPCVIEW(child, driver, onlyMetrics);
}


static void 
ProcessHPCVIEW(DOMNode *node, Driver &driver, bool onlyMetrics)
{
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << node);

  if ((node == NULL) ||
      (node->getNodeType() != DOMNode::DOCUMENT_TYPE_NODE) ){ 
    ConfigParser_Throw("CONFIGURATION file does not begin with <HPCVIEW>");
  };
  
  node = node->getNextSibling();
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << node);

  if ( (node == NULL)
       || (node->getNodeType() != DOMNode::ELEMENT_NODE)) {
    ConfigParser_Throw("No DOCUMENT_NODE found in CONFIGURATION file.");
  };

  // process each child 
  DOMNode *child = node->getFirstChild();
  for (; child != NULL; child = child->getNextSibling()) {
    ProcessELEMENT(child, driver, onlyMetrics);
  }
} 


static void 
ProcessELEMENT(DOMNode *node, Driver &driver, bool onlyMetrics)
{
  static XMLCh* METRIC       = XMLString::transcode("METRIC");
  static XMLCh* PATH         = XMLString::transcode("PATH");
  static XMLCh* REPLACE      = XMLString::transcode("REPLACE");
  static XMLCh* TITLE        = XMLString::transcode("TITLE");
  static XMLCh* STRUCTURE    = XMLString::transcode("STRUCTURE");
  static XMLCh* GROUP        = XMLString::transcode("GROUP");
  static XMLCh* NAMEATTR     = XMLString::transcode("name");
  static XMLCh* VIEWNAMEATTR = XMLString::transcode("viewname");
  static XMLCh* INATTR       = XMLString::transcode("in");
  static XMLCh* OUTATTR      = XMLString::transcode("out");

  const XMLCh *nodeName = node->getNodeName();

  // Verify node type
  if (node->getNodeType() == DOMNode::TEXT_NODE) {
    return; // DTD ensures this can't contain anything but white space
  }
  else if (node->getNodeType() == DOMNode::COMMENT_NODE) {
    return;
  }
  else if (node->getNodeType() != DOMNode::ELEMENT_NODE) {
    ConfigParser_Throw("Unexpected XML object found: '" << XMLString::transcode(nodeName) << "'");
  }

  // Parse ELEMENT nodes
  if (XMLString::equals(nodeName,PATH)) {  
    if (!onlyMetrics) {
      string path = getAttr(node, NAMEATTR);
      string viewname = getAttr(node, VIEWNAMEATTR);
      DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "PATH: " << path << ", v=" << viewname);
      
      if (path.empty()) {
	ConfigParser_Throw("PATH name attribute cannot be empty.");
      }
      else if (driver.CopySrcFiles() && viewname.empty()) {
	ConfigParser_Throw("PATH '" << path << "': viewname attribute cannot be empty when source files are to be copied.");
      } // there could be many other nefarious values of these attributes
      
      driver.AddSearchPath(path, viewname);
    }
  }
  else if (XMLString::equals(nodeName,REPLACE)) {  
    if (!onlyMetrics) {
      string inPath = getAttr(node, INATTR); 
      string outPath = getAttr(node, OUTATTR); 
      DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "REPLACE: " << inPath << " -to- " << outPath);
      
      bool addPath = true;
      if (inPath == outPath) {
	cerr << "WARNING: REPLACE: Identical 'in' and 'out' paths: '"
	     << inPath << "'!  No action will be performed." << endl;
	addPath = false;
      }
      else if (inPath.empty() && !outPath.empty()) {
	cerr << "WARNING: REPLACE: 'in' path is empty; 'out' path '" << outPath
	     << "' will be prepended to every file path!" << endl;
      }
      
      if (addPath) { driver.AddReplacePath(inPath, outPath); }
    }
  }
  else if (XMLString::equals(nodeName,METRIC)) {
    if (onlyMetrics) {
      ProcessMETRIC(node, driver);
    }
  }
  else if (XMLString::equals(nodeName,TITLE)) {
    if (!onlyMetrics) {
      string title = getAttr(node, NAMEATTR); 
      DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "TITLE: " << title);
      driver.SetTitle(title);
    }
  }
  else if (XMLString::equals(nodeName,STRUCTURE)) {
    if (!onlyMetrics) {
      string fnm = getAttr(node, NAMEATTR); // file name
      DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "STRUCTURE file: " << fnm);
      
      if (fnm.empty()) {
	ConfigParser_Throw("STRUCTURE file name is empty.");
      } 
      else {
	driver.AddStructureFile(fnm);
      }
    }
  } 
  else if (XMLString::equals(nodeName,GROUP)) {
    if (!onlyMetrics) {
      string fnm = getAttr(node, NAMEATTR); // file name
      DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "GROUP file: " << fnm);
      
      if (fnm.empty()) {
	ConfigParser_Throw("GROUP file name is empty.");
      } 
      else {
	driver.AddGroupFile(fnm);
      }
    }
  } 
  else {
    ConfigParser_Throw("Unexpected ELEMENT type encountered: '" << XMLString::transcode(nodeName) << "'");
  }
}


static void 
ProcessMETRIC(DOMNode *node, Driver &driver)
{
  static XMLCh* FILE = XMLString::transcode("FILE");
  static XMLCh* COMPUTE = XMLString::transcode("COMPUTE");
  static XMLCh* NAMEATTR = XMLString::transcode("name");
  static XMLCh* DISPLAYATTR = XMLString::transcode("display");
  static XMLCh* PERCENTATTR = XMLString::transcode("percent");
  static XMLCh* PROPAGATEATTR = XMLString::transcode("propagate");
  static XMLCh* DISPLAYNAMEATTR = XMLString::transcode("displayName");
  static XMLCh* SORTBYATTR = XMLString::transcode("sortBy");

  // get metric attributes
  string metricName = getAttr(node, NAMEATTR); 
  string metricDisplayName = getAttr(node, DISPLAYNAMEATTR); 
  bool metricDoDisplay = (getAttr(node, DISPLAYATTR) == "true"); 
  bool metricDoPercent = (getAttr(node,PERCENTATTR) == "true"); 
  bool metricDoSortBy = (getAttr(node,SORTBYATTR) == "true"); 

  if (metricName.empty()) {
    ConfigParser_Throw("METRIC: Invalid name: '" << metricName << "'.");
  }
  else if (NameToPerfDataIndex(metricName) != UNDEF_METRIC_INDEX) {
    ConfigParser_Throw("METRIC: Metric name '" << metricName << "' was previously defined.");
  }

  if (metricDisplayName.empty()) {
    ConfigParser_Throw("METRIC: Invalid displayName: '" << metricDisplayName << "'.");
  }
    
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "METRIC: name=" << metricName
		<< " display=" <<  ((metricDoDisplay) ? "true" : "false")
		<< " doPercent=" <<  ((metricDoPercent) ? "true" : "false")
		<< " sortBy=" <<  ((metricDoSortBy) ? "true" : "false")
		<< " metricDisplayName=" << metricDisplayName);
		
  // should have exactly one child
  DOMNode *metricImpl = node->getFirstChild();

  for ( ; metricImpl != NULL; metricImpl = metricImpl->getNextSibling()) {

    if (metricImpl->getNodeType() == DOMNode::TEXT_NODE) {
      // DTD ensures this can't contain anything but white space
      continue;
    }
    else if (metricImpl->getNodeType() == DOMNode::COMMENT_NODE) {
      continue;
    }
    
    const XMLCh *metricType = metricImpl->getNodeName();

    if (XMLString::equals(metricType,FILE)) {

      ProcessFILE(metricImpl, driver, metricName, 
		  metricDoDisplay, metricDoPercent, metricDoSortBy, 
		  metricDisplayName);
    }
    else if (XMLString::equals(metricType,COMPUTE)) {
      bool propagateComputed
	= (getAttr(metricImpl,PROPAGATEATTR) == "computed"); 
      DOMNode *child = metricImpl->getFirstChild();
      for (; child != NULL; child = child->getNextSibling()) {
	if (child->getNodeType() == DOMNode::TEXT_NODE) {
	  // DTD ensures this can't contain anything but white space
	  continue;
	}
	else if (child->getNodeType() == DOMNode::COMMENT_NODE) {
	  continue;
	}
	
	driver.Add(new ComputedPerfMetric(metricName, metricDisplayName, 
					  metricDoDisplay, metricDoPercent, 
					  metricDoSortBy,
					  propagateComputed, child)); 
      }
    } 
    else {
      ConfigParser_Throw("Unexpected METRIC type '" << XMLString::transcode(metricType) << "'.");
    }
  }
}


static void 
ProcessFILE(DOMNode* fileNode, Driver& driver, 
	    const string& metricName, bool metricDoDisplay, 
	    bool metricDoPercent, bool metricDoSortBy, 
	    const string& metricDisplayName)
{
  static XMLCh* TYPEATTR = XMLString::transcode("type");
  static XMLCh* NAMEATTR = XMLString::transcode("name");
  static XMLCh* SELECTATTR = XMLString::transcode("select");

  string metricFile     = getAttr(fileNode, NAMEATTR); 
  string metricFileType = getAttr(fileNode, TYPEATTR);
  string nativeName     = getAttr(fileNode, SELECTATTR);
  
  if (nativeName.empty()) {
    //    string error = "FILE: Invalid select attribute '" + nativeName + "'";
    //    throw DocException(error);
    //  Default is to select "0"
    nativeName = "0";
  }

  if (!metricFile.empty()) { 
    driver.Add(new FilePerfMetric(metricName, nativeName, metricDisplayName, 
				  metricDoDisplay, metricDoPercent, 
				  metricDoSortBy, metricFile, metricFileType, 
				  &driver));
  } 
  else {
    ConfigParser_Throw("METRIC '" << metricName << "' FILE name empty.");
  }

}


// -------------------------------------------------------------------------

// Returns the attribute value or an empty string if the attribute
// does not exist.
static string 
getAttr(DOMNode *node, const XMLCh *attrName)
{
  DOMNamedNodeMap  *attributes = node->getAttributes();
  DOMNode *attrNode = attributes->getNamedItem(attrName);
  DIAG_DevMsgIf(DBG_ME, "HPCVIEW: " << "getAttr(): attrNode" << attrNode);
  if (attrNode == NULL) {
    return "";
  } 
  else {
    const XMLCh *attrNodeValue = attrNode->getNodeValue();
    return XMLString::transcode(attrNodeValue);
  }
}

