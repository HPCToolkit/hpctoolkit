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

//************************* User Include Files *******************************

#include "Driver.h"
#include "MathMLExpr.h"
#include "DerivedPerfMetrics.h"
#include "PerfMetric.h"
#include "ScopesInfo.h"
#include "XMLAdapter.h"

#include <lib/support/String.h>
#include <lib/support/Assertion.h>
#include <lib/support/Trace.h>

#include <xercesc/dom/deprecated/DOM_Node.hpp> /* FIXME */

//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************

// -------------------------------------------------------------------------
// local types 
// -------------------------------------------------------------------------

class DocException {
public:
  DocException (String msg) {
    msgtext = msg;
  }
  String message() const { 
    return msgtext; 
  }
private:
  String msgtext;
};


// -------------------------------------------------------------------------
// declare forward references 
// -------------------------------------------------------------------------

static void ProcessDOCUMENT(DOM_Node &node, Driver &driver);
static void ProcessHPCVIEW(DOM_Node &node, Driver &driver);
static void ProcessELEMENT(DOM_Node &node, Driver &driver);
static void ProcessMETRIC(DOM_Node &node, Driver &driver);
static void ProcessFILE(DOM_Node &fileNode, Driver &driver, 
			String &metricName, bool metricDoDisplay, 
			bool metricDoPercent, bool metricDoSortBy, 
			String &metricDisplayName);

static String getAttr(DOM_Node &node, const DOMString &attrName);

// -------------------------------------------------------------------------
// external operations
// -------------------------------------------------------------------------

void PrintName(DOM_Node node)
{
  DOMString nodeName = node.getNodeName();
  DOMString nodeValue = node.getNodeValue();
  cerr << "name=" << nodeName.transcode() 
       << " value=" << nodeValue.transcode() << endl;
}

void HPCViewDocParser(Driver &driver, String &inputFile, 
		      ErrorHandler &errReporter)
{
  DOMParser parser;
  parser.setDoValidation(true);

  parser.setErrorHandler(&errReporter);

  try { 
    IFTRACE << "Parsing configuration file: " << inputFile << endl; 

    // unless an error is thrown, parsing is successful
    parser.parse(inputFile); 

    DOM_Node doc = parser.getDocument();
    IFTRACE << "document: " << doc << endl;

    ProcessDOCUMENT(doc, driver);
  }
  catch (const DocException &d) {
    cerr << "HPCView configuration file error: " << d.message() << endl;
    //eraxxon: error: const Locator* loc = (parser.getScanner()).getLocator();
    exit(-1);
  }
  catch (...) {
    cerr << "Unexpected error encountered!";
    exit(-1);
  }

}

// -------------------------------------------------------------------------
// internal operations
// -------------------------------------------------------------------------

static void ProcessDOCUMENT(DOM_Node &node, Driver &driver)
{
  DOM_Node child = node.getFirstChild();
  ProcessHPCVIEW(child,driver);
}

static void ProcessHPCVIEW(DOM_Node &node, Driver &driver)
{
  DOMString nodeValue = node.getNodeValue();

  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ((node == NULL) ||
	  (node.getNodeType() != DOM_Node::DOCUMENT_TYPE_NODE) ){ 
    String error = "Config file does not begin with DOCUMENT_TYPE_NODE"; 
    throw DocException(error); 
  };
  
  node = node.getNextSibling();
  IFTRACE << "HPCVIEW:" << endl << node << endl; 

  if ( (node == NULL)
       || (node.getNodeType() != DOM_Node::ELEMENT_NODE)) {
    String error = "No DOCUMENT_NODE found in config file."; 
    throw DocException(error); 
  };

  // process each child 
  DOM_Node child = node.getFirstChild();
  for (; child != NULL; child = child.getNextSibling()) {
    ProcessELEMENT(child, driver);
  }
} 


static void ProcessELEMENT(DOM_Node &node, Driver &driver)
{
  static DOMString METRIC("METRIC");
  static DOMString PATH("PATH");
  static DOMString REPLACE("REPLACE");
  static DOMString TITLE("TITLE");
  static DOMString STRUCTURE("STRUCTURE");
  static DOMString NAMEATTR("name");
  static DOMString VIEWNAMEATTR("viewname");
  static DOMString INATTR("in");
  static DOMString OUTATTR("out");

  DOMString nodeName = node.getNodeName();

  // Verify node type
  if (node.getNodeType() == DOM_Node::TEXT_NODE) {
    return; // DTD ensures this can't contain anything but white space
  } else if (node.getNodeType() == DOM_Node::COMMENT_NODE) {
    return;
  } else if (node.getNodeType() != DOM_Node::ELEMENT_NODE) {
    String error = "Unexpected XML object found: '" +
      String(nodeName.transcode()) + "'";
    throw DocException(error); 
  }

  // Parse ELEMENT nodes
  if (nodeName.equals(PATH)) {  
    String path = getAttr(node, NAMEATTR);
    String viewname = getAttr(node, VIEWNAMEATTR);
    IFTRACE << "PATH: " << path << ", v=" << viewname << endl; 
    
    if (path.Length() == 0) {
      String error = "PATH: path attribute cannot be empty.";
      throw DocException(error); 
    } else if (driver.CopySrcFiles() && viewname.Length() == 0) {
      String error = "PATH '" + path + "': viewname attribute cannot be empty when source files are to be copied.";
      throw DocException(error); 
    } // there could be many other nefarious values of these attributes
    
    driver.AddPath(path, viewname);
    
  } else if (nodeName.equals(REPLACE)) {  
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
    
  } else if (nodeName.equals(METRIC)) {
    ProcessMETRIC(node, driver);

  } else if (nodeName.equals(TITLE)) {
    String title = getAttr(node, NAMEATTR); 
    IFTRACE << "TITLE: " << title << endl; 
    driver.SetTitle(title);

  } else if (nodeName.equals(STRUCTURE)) {
    String pgmFileName = getAttr(node, NAMEATTR); 
    IFTRACE << "STRUCTURE file: " << pgmFileName << endl; 

    if ( driver.IsPGMFileAvailable() ) {
      String error = "ERROR: STRUCTURE file previously defined as " + 
	String(driver.PGMFileName()) + ", cannot set to " + pgmFileName;
      throw DocException(error); 
    } else if ( pgmFileName.Length() == 0 ) {
      String error = "ERROR: STRUCTURE file name is empty.";
      throw DocException(error); 
    } 
    else {
      driver.SetPGMFileName(pgmFileName);
    }
  } else {
    String error = "Unexpected ELEMENT type encountered: '" +
      String(nodeName.transcode()) + "'";
    throw DocException(error);
  }
}

static void ProcessMETRIC(DOM_Node &node, Driver &driver)
{
  static DOMString FILE("FILE");
  static DOMString COMPUTE("COMPUTE");
  static DOMString NAMEATTR("name");
  static DOMString DISPLAYATTR("display");
  static DOMString PERCENTATTR("percent");
  static DOMString PROPAGATEATTR("propagate");
  static DOMString DISPLAYNAMEATTR("displayName");
  static DOMString SORTBYATTR("sortBy");

  // get metric attributes
  String metricName = getAttr(node, NAMEATTR); 
  String metricDisplayName = getAttr(node, DISPLAYNAMEATTR); 
  bool metricDoDisplay = (getAttr(node, DISPLAYATTR) == "true"); 
  bool metricDoPercent = (getAttr(node,PERCENTATTR) == "true"); 
  bool metricDoSortBy = (getAttr(node,SORTBYATTR) == "true"); 

  if (metricName.Length() == 0) {
    String error = "METRIC: Invalid name: '" + metricName + "'.";
    throw DocException(error); 
  } else if (NameToPerfDataIndex(metricName) != UNDEF_METRIC_INDEX) {
    String error = String("METRIC: Metric name '") + metricName +
      "' was previously defined.";
    throw DocException(error); 
  }

  if (metricDisplayName.Length() == 0) {
    String error = "METRIC: Invalid displayName: '" + metricDisplayName + "'.";
    throw DocException(error); 
  }
    
  IFTRACE << "METRIC: name=" << metricName << " " 
	  << "display=" <<  ((metricDoDisplay) ? "true" : "false") << " " 
	  << "doPercent=" <<  ((metricDoPercent) ? "true" : "false") << " " 
	  << "sortBy=" <<  ((metricDoSortBy) ? "true" : "false") << " " 
	  << "metricDisplayName=" << metricDisplayName << " " 
	  << endl;
  
  // should have exactly one child
  DOM_Node metricImpl = node.getFirstChild();

  for ( ; metricImpl != NULL; metricImpl = metricImpl.getNextSibling()) {

    if (metricImpl.getNodeType() == DOM_Node::TEXT_NODE) {
      // DTD ensures this can't contain anything but white space
      continue;
    } else if (metricImpl.getNodeType() == DOM_Node::COMMENT_NODE) {
      continue;
    }
    
    DOMString metricType = metricImpl.getNodeName();

    if (metricType.equals(FILE)) {

      ProcessFILE(metricImpl, driver, metricName, 
		  metricDoDisplay, metricDoPercent, metricDoSortBy, 
		  metricDisplayName);
    } else if (metricType.equals(COMPUTE)) {
      bool propagateComputed
	= (getAttr(metricImpl,PROPAGATEATTR) == "computed"); 
      DOM_Node child = metricImpl.getFirstChild();
      for (; child != NULL; child = child.getNextSibling()) {
	if (child.getNodeType() == DOM_Node::TEXT_NODE) {
	  // DTD ensures this can't contain anything but white space
	  continue;
	} else if (child.getNodeType() == DOM_Node::COMMENT_NODE) {
	  continue;
	}
	
	driver.Add(new ComputedPerfMetric(metricName, metricDisplayName, 
					  metricDoDisplay, metricDoPercent, metricDoSortBy,
					  propagateComputed, child)); 
      }
    
    } else {
      String error = String("Unexpected METRIC type '") + 
	metricType.transcode() + "'.";
      throw DocException(error);
    }
  }
}

static void ProcessFILE(DOM_Node &fileNode, Driver &driver, 
			String &metricName, bool metricDoDisplay, 
			bool metricDoPercent, bool metricDoSortBy, 
			String &metricDisplayName)
{
  static DOMString TYPEATTR("type");
  static DOMString NAMEATTR("name");
  static DOMString SELECTATTR("select");

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
    String error = "FILE: Invalid filename '" + metricFile + "'";
    throw DocException(error);
  }

}

// -------------------------------------------------------------------------

// Returns the attribute value or an empty string if the attribute
// does not exist.
static String getAttr(DOM_Node &node, const DOMString &attrName)
{
  DOM_NamedNodeMap  attributes = node.getAttributes();
  DOM_Node attrNode = attributes.getNamedItem(attrName);
  IFTRACE << "getAttr(): attrNode" << attrNode << endl;
  if (attrNode.isNull()) {
    return String("");
  } else {
    DOMString attrNodeValue = attrNode.getNodeValue();
    return String(attrNodeValue.transcode());
  }
}

