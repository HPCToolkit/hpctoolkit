// $Id$
// -*-C++-*-
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

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib> // for 'exit'
using std::exit; // For compatibility with non-std C headers
#endif

//************************* Xerces Include Files *******************************

#include <xercesc/util/XMLString.hpp> 
using XERCES_CPP_NAMESPACE::XMLString;

#include <xercesc/parsers/XercesDOMParser.hpp>
using XERCES_CPP_NAMESPACE::XercesDOMParser;

#include <xercesc/dom/DOMNamedNodeMap.hpp> 
using XERCES_CPP_NAMESPACE::DOMNamedNodeMap;


//************************* User Include Files *******************************

#include "HPCViewDocParser.h"

#include "Driver.h"
#include "MathMLExpr.h"
#include "DerivedPerfMetrics.h"
#include "PerfMetric.h"
#include "ScopesInfo.h"

#include <lib/support/Trace.h>


//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************


// -------------------------------------------------------------------------
// declare forward references 
// -------------------------------------------------------------------------

static void ProcessDOCUMENT(DOMNode *node, Driver &driver);
static void ProcessHPCVIEW(DOMNode *node, Driver &driver);
static void ProcessELEMENT(DOMNode *node, Driver &driver);
static void ProcessMETRIC(DOMNode *node, Driver &driver);
static void ProcessFILE(DOMNode *fileNode, Driver &driver, 
			String &metricName, bool metricDoDisplay, 
			bool metricDoPercent, bool metricDoSortBy, 
			String &metricDisplayName);

static String getAttr(DOMNode *node, const XMLCh *attrName);

// -------------------------------------------------------------------------
// external operations
// -------------------------------------------------------------------------

void PrintName(DOMNode *node)
{
  const XMLCh *nodeName = node->getNodeName();
  const XMLCh *nodeValue = node->getNodeValue();
  cerr << "name=" << XMLString::transcode(nodeName) 
       << " value=" << XMLString::transcode(nodeValue) << endl;
}

void HPCViewDocParser(Driver &driver, String &inputFile, 
		      HPCViewXMLErrHandler  &errReporter)
{
  XercesDOMParser *parser = new XercesDOMParser;

  parser->setValidationScheme(XercesDOMParser::Val_Auto);

  parser->setErrorHandler(&errReporter);

  IFTRACE << "Parsing configuration file: " << inputFile << endl; 
  
  parser->parse(inputFile); 
  if (parser->getErrorCount() > 0) {
    String error =  "terminating because of previously reported CONFIGURATION file parse errors.";
    throw HPCViewDocException(error); 
  }
  
  DOMNode *doc = parser->getDocument();
  IFTRACE << "document: " << doc << endl;
  
  ProcessDOCUMENT(doc, driver);

  delete parser;
}

// -------------------------------------------------------------------------
// internal operations
// -------------------------------------------------------------------------

static void ProcessDOCUMENT(DOMNode *node, Driver &driver)
{
  DOMNode *child = node->getFirstChild();
  ProcessHPCVIEW(child,driver);
}

static void ProcessHPCVIEW(DOMNode *node, Driver &driver)
{
  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ((node == NULL) ||
	  (node->getNodeType() != DOMNode::DOCUMENT_TYPE_NODE) ){ 
    String error = "CONFIGURATION file does not begin with <HPCVIEW>"; 
    throw HPCViewDocException(error); 
  };
  
  node = node->getNextSibling();
  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ( (node == NULL)
       || (node->getNodeType() != DOMNode::ELEMENT_NODE)) {
    String error = "No DOCUMENT_NODE found in CONFIGURATION file."; 
    throw HPCViewDocException(error); 
  };

  // process each child 
  DOMNode *child = node->getFirstChild();
  for (; child != NULL; child = child->getNextSibling()) {
    ProcessELEMENT(child, driver);
  }
} 


static void ProcessELEMENT(DOMNode *node, Driver &driver)
{
  static XMLCh *METRIC = XMLString::transcode("METRIC");
  static XMLCh *PATH = XMLString::transcode("PATH");
  static XMLCh *REPLACE = XMLString::transcode("REPLACE");
  static XMLCh *TITLE = XMLString::transcode("TITLE");
  static XMLCh *STRUCTURE = XMLString::transcode("STRUCTURE");
  static XMLCh *NAMEATTR = XMLString::transcode("name");
  static XMLCh *VIEWNAMEATTR = XMLString::transcode("viewname");
  static XMLCh *INATTR = XMLString::transcode("in");
  static XMLCh *OUTATTR = XMLString::transcode("out");

  const XMLCh *nodeName = node->getNodeName();

  // Verify node type
  if (node->getNodeType() == DOMNode::TEXT_NODE) {
    return; // DTD ensures this can't contain anything but white space
  } else if (node->getNodeType() == DOMNode::COMMENT_NODE) {
    return;
  } else if (node->getNodeType() != DOMNode::ELEMENT_NODE) {
    String error = "Unexpected XML object found: '" +
      String(XMLString::transcode(nodeName)) + "'";
    throw HPCViewDocException(error); 
  }

  // Parse ELEMENT nodes
  if (XMLString::equals(nodeName,PATH)) {  
    String path = getAttr(node, NAMEATTR);
    String viewname = getAttr(node, VIEWNAMEATTR);
    IFTRACE << "PATH: " << path << ", v=" << viewname << endl; 
    
    if (path.Length() == 0) {
      String error = "PATH name attribute cannot be empty.";
      throw HPCViewDocException(error); 
    } else if (driver.CopySrcFiles() && viewname.Length() == 0) {
      String error = "PATH '" + path + "': viewname attribute cannot be empty when source files are to be copied.";
      throw HPCViewDocException(error); 
    } // there could be many other nefarious values of these attributes
    
    driver.AddPath(path, viewname);
    
  } else if (XMLString::equals(nodeName,REPLACE)) {  
    String inPath = getAttr(node, INATTR); 
    String outPath = getAttr(node, OUTATTR); 
    IFTRACE << "REPLACE: " << inPath << " -to- " << outPath << endl;

    bool addPath = true;
    if (strcmp(inPath, outPath) == 0) {
      cerr << "WARNING: REPLACE: Identical 'in' and 'out' paths: '"
	   << inPath << "'!  No action will be performed." << endl;
      addPath = false;
    } else if (inPath.Length() == 0 && outPath.Length() != 0) {
      cerr << "WARNING: REPLACE: 'in' path is empty; 'out' path '" << outPath
	   << "' will be prepended to every file path!" << endl;
    }

    if (addPath) { driver.AddReplacePath(inPath, outPath); }
    
  } else if (XMLString::equals(nodeName,METRIC)) {
    ProcessMETRIC(node, driver);

  } else if (XMLString::equals(nodeName,TITLE)) {
    String title = getAttr(node, NAMEATTR); 
    IFTRACE << "TITLE: " << title << endl; 
    driver.SetTitle(title);

  } else if (XMLString::equals(nodeName,STRUCTURE)) {
    String pgmFileName = getAttr(node, NAMEATTR); 
    IFTRACE << "STRUCTURE file: " << pgmFileName << endl; 

    if ( pgmFileName.Length() == 0 ) {
      String error = "STRUCTURE file name is empty.";
      throw HPCViewDocException(error); 
    } 
    else {
      driver.SetPGMFileName(pgmFileName);
    }
  } else {
    String error = "Unexpected ELEMENT type encountered: '" +
      String(XMLString::transcode(nodeName)) + "'";
    throw HPCViewDocException(error);
  }
}

static void ProcessMETRIC(DOMNode *node, Driver &driver)
{
  static XMLCh *FILE = XMLString::transcode("FILE");
  static XMLCh *COMPUTE = XMLString::transcode("COMPUTE");
  static XMLCh *NAMEATTR = XMLString::transcode("name");
  static XMLCh *DISPLAYATTR = XMLString::transcode("display");
  static XMLCh *PERCENTATTR = XMLString::transcode("percent");
  static XMLCh *PROPAGATEATTR = XMLString::transcode("propagate");
  static XMLCh *DISPLAYNAMEATTR = XMLString::transcode("displayName");
  static XMLCh *SORTBYATTR = XMLString::transcode("sortBy");

  // get metric attributes
  String metricName = getAttr(node, NAMEATTR); 
  String metricDisplayName = getAttr(node, DISPLAYNAMEATTR); 
  bool metricDoDisplay = (getAttr(node, DISPLAYATTR) == "true"); 
  bool metricDoPercent = (getAttr(node,PERCENTATTR) == "true"); 
  bool metricDoSortBy = (getAttr(node,SORTBYATTR) == "true"); 

  if (metricName.Length() == 0) {
    String error = "METRIC: Invalid name: '" + metricName + "'.";
    throw HPCViewDocException(error); 
  } else if (NameToPerfDataIndex(metricName) != UNDEF_METRIC_INDEX) {
    String error = String("METRIC: Metric name '") + metricName +
      "' was previously defined.";
    throw HPCViewDocException(error); 
  }

  if (metricDisplayName.Length() == 0) {
    String error = "METRIC: Invalid displayName: '" + metricDisplayName + "'.";
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
    } else if (metricImpl->getNodeType() == DOMNode::COMMENT_NODE) {
      continue;
    }
    
    const XMLCh *metricType = metricImpl->getNodeName();

    if (XMLString::equals(metricType,FILE)) {

      ProcessFILE(metricImpl, driver, metricName, 
		  metricDoDisplay, metricDoPercent, metricDoSortBy, 
		  metricDisplayName);
    } else if (XMLString::equals(metricType,COMPUTE)) {
      bool propagateComputed
	= (getAttr(metricImpl,PROPAGATEATTR) == "computed"); 
      DOMNode *child = metricImpl->getFirstChild();
      for (; child != NULL; child = child->getNextSibling()) {
	if (child->getNodeType() == DOMNode::TEXT_NODE) {
	  // DTD ensures this can't contain anything but white space
	  continue;
	} else if (child->getNodeType() == DOMNode::COMMENT_NODE) {
	  continue;
	}
	
	driver.Add(new ComputedPerfMetric(metricName, metricDisplayName, 
					  metricDoDisplay, metricDoPercent, metricDoSortBy,
					  propagateComputed, child)); 
      }
    
    } else {
      String error = String("Unexpected METRIC type '") + 
	XMLString::transcode(metricType) + "'.";
      throw HPCViewDocException(error);
    }
  }
}

static void ProcessFILE(DOMNode *fileNode, Driver &driver, 
			String &metricName, bool metricDoDisplay, 
			bool metricDoPercent, bool metricDoSortBy, 
			String &metricDisplayName)
{
  static XMLCh *TYPEATTR = XMLString::transcode("type");
  static XMLCh *NAMEATTR = XMLString::transcode("name");
  static XMLCh *SELECTATTR = XMLString::transcode("select");

  String metricFile     = getAttr(fileNode, NAMEATTR); 
  String metricFileType = getAttr(fileNode, TYPEATTR);
  String nativeName     = getAttr(fileNode, SELECTATTR);
  
  if (nativeName.Length() == 0) {
    //    String error = "FILE: Invalid select attribute '" + nativeName + "'";
    //    throw DocException(error);
    //  Default is to select "0"
    nativeName = "0";
  }

  if (metricFile.Length() > 0) { 
    driver.Add(new FilePerfMetric(metricName, nativeName, metricDisplayName, 
				  metricDoDisplay, metricDoPercent, metricDoSortBy, 
				  metricFile, metricFileType, &driver)); 
  } else {
    String error = "METRIC '" + metricName + "' FILE name empty.";
    throw HPCViewDocException(error);
  }

}

// -------------------------------------------------------------------------

// Returns the attribute value or an empty string if the attribute
// does not exist.
static String getAttr(DOMNode *node, const XMLCh *attrName)
{
  DOMNamedNodeMap  *attributes = node->getAttributes();
  DOMNode *attrNode = attributes->getNamedItem(attrName);
  IFTRACE << "getAttr(): attrNode" << attrNode << endl;
  if (attrNode == NULL) {
    return String("");
  } else {
    const XMLCh *attrNodeValue = attrNode->getNodeValue();
    return String(XMLString::transcode(attrNodeValue));
  }
}

