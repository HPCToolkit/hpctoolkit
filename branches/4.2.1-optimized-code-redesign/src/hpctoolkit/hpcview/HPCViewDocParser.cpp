// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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

#include "HPCViewDocParser.hpp"

#include "Driver.hpp"
#include "MathMLExpr.hpp"
#include "DerivedPerfMetrics.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>
#include <lib/prof-juicy/PerfMetric.hpp>

#include <lib/support/Trace.hpp>


//************************ Forward Declarations ******************************

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

HPCViewDocParser::HPCViewDocParser(const string& inputFile, 
				   HPCViewXMLErrHandler& errHndlr)
{
  IFTRACE << "Parsing configuration file: " << inputFile << endl; 

  mParser = new XercesDOMParser;
  mParser->setValidationScheme(XercesDOMParser::Val_Auto);
  mParser->setErrorHandler(&errHndlr);
  mParser->parse(inputFile.c_str());
  if (mParser->getErrorCount() > 0) {
    HPCViewDoc_Throw("terminating because of previously reported CONFIGURATION file parse errors.");
  }
  
  mDoc = mParser->getDocument();
  IFTRACE << "document: " << mDoc << endl;
}


HPCViewDocParser::~HPCViewDocParser()
{
  delete mParser;
}


void
HPCViewDocParser::pass1(Driver& driver)
{
  ProcessDOCUMENT(mDoc, driver, false /*onlyMetrics*/);
}


void
HPCViewDocParser::pass2(Driver& driver)
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
  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ((node == NULL) ||
      (node->getNodeType() != DOMNode::DOCUMENT_TYPE_NODE) ){ 
    HPCViewDoc_Throw("CONFIGURATION file does not begin with <HPCVIEW>");
  };
  
  node = node->getNextSibling();
  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ( (node == NULL)
       || (node->getNodeType() != DOMNode::ELEMENT_NODE)) {
    HPCViewDoc_Throw("No DOCUMENT_NODE found in CONFIGURATION file.");
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
    string error = "Unexpected XML object found: '" +
      string(XMLString::transcode(nodeName)) + "'";
    throw HPCViewDocException(error); 
  }

  // Parse ELEMENT nodes
  if (XMLString::equals(nodeName,PATH)) {  
    if (!onlyMetrics) {
      string path = getAttr(node, NAMEATTR);
      string viewname = getAttr(node, VIEWNAMEATTR);
      IFTRACE << "PATH: " << path << ", v=" << viewname << endl; 
      
      if (path.empty()) {
	string error = "PATH name attribute cannot be empty.";
	throw HPCViewDocException(error); 
      }
      else if (driver.CopySrcFiles() && viewname.empty()) {
	string error = "PATH '" + path + "': viewname attribute cannot be empty when source files are to be copied.";
	throw HPCViewDocException(error); 
      } // there could be many other nefarious values of these attributes
      
      driver.AddPath(path, viewname);
    }
  }
  else if (XMLString::equals(nodeName,REPLACE)) {  
    if (!onlyMetrics) {
      string inPath = getAttr(node, INATTR); 
      string outPath = getAttr(node, OUTATTR); 
      IFTRACE << "REPLACE: " << inPath << " -to- " << outPath << endl;
      
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
      IFTRACE << "TITLE: " << title << endl; 
      driver.SetTitle(title);
    }
  }
  else if (XMLString::equals(nodeName,STRUCTURE)) {
    if (!onlyMetrics) {
      string fnm = getAttr(node, NAMEATTR); // file name
      IFTRACE << "STRUCTURE file: " << fnm << endl; 
      
      if (fnm.empty()) {
	string error = "STRUCTURE file name is empty.";
	throw HPCViewDocException(error); 
      } 
      else {
	driver.AddStructureFile(fnm);
      }
    }
  } 
  else if (XMLString::equals(nodeName,GROUP)) {
    if (!onlyMetrics) {
      string fnm = getAttr(node, NAMEATTR); // file name
      IFTRACE << "GROUP file: " << fnm << endl; 
      
      if (fnm.empty()) {
	string error = "GROUP file name is empty.";
	throw HPCViewDocException(error); 
      } 
      else {
	driver.AddGroupFile(fnm);
      }
    }
  } 
  else {
    string error = "Unexpected ELEMENT type encountered: '" +
      string(XMLString::transcode(nodeName)) + "'";
    throw HPCViewDocException(error);
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
    string error = "METRIC: Invalid name: '" + metricName + "'.";
    throw HPCViewDocException(error); 
  }
  else if (NameToPerfDataIndex(metricName) != UNDEF_METRIC_INDEX) {
    string error = "METRIC: Metric name '" + metricName +
      "' was previously defined.";
    throw HPCViewDocException(error); 
  }

  if (metricDisplayName.empty()) {
    string error = "METRIC: Invalid displayName: '" + metricDisplayName + "'.";
    throw HPCViewDocException(error); 
  }
    
  IFTRACE << "METRIC: name=" << metricName << " " 
	  << "display=" <<  ((metricDoDisplay) ? "true" : "false") << " " 
	  << "doPercent=" <<  ((metricDoPercent) ? "true" : "false") << " " 
	  << "sortBy=" <<  ((metricDoSortBy) ? "true" : "false") << " " 
	  << "metricDisplayName=" << metricDisplayName << " " 
	  << endl;
  
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
      string error = string("Unexpected METRIC type '") + 
	XMLString::transcode(metricType) + "'.";
      throw HPCViewDocException(error);
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
    string error = "METRIC '" + metricName + "' FILE name empty.";
    throw HPCViewDocException(error);
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
  IFTRACE << "getAttr(): attrNode" << attrNode << endl;
  if (attrNode == NULL) {
    return "";
  } 
  else {
    const XMLCh *attrNodeValue = attrNode->getNodeValue();
    return XMLString::transcode(attrNodeValue);
  }
}

