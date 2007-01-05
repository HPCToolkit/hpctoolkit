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

// ----------------------------------------------------------------------
//
// XMLStr.h
//   the header file for a string converter class which converts a
//   string to XMLCh* or vice versa.
//
// ----------------------------------------------------------------------

#ifndef XMLAdapter_h
#define XMLAdapter_h

//************************ System Include Files ******************************

#include <iostream> 

//************************* User Include Files *******************************

#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/dom/deprecated/DOMParser.hpp>        /* FIXME */

#include <xercesc/sax/AttributeList.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/util/XMLUniDefs.hpp>

#include <xercesc/dom/deprecated/DOM.hpp>              /* FIXME */
#include <xercesc/dom/deprecated/DOMString.hpp>        /* FIXME */
#include <xercesc/dom/deprecated/DOM_Node.hpp>         /* FIXME */
#include <xercesc/dom/deprecated/DOM_NamedNodeMap.hpp> /* FIXME */

#include <xercesc/framework/MemBufInputSource.hpp>

#include <include/general.h>
#include <lib/support/String.h>

//************************ Forward Declarations ******************************

//****************************************************************************

std::string toString(DOM_Node& node);

std::ostream&
dumpDomNode(DOM_Node& node, std::ostream& os = std::cerr);

void 
ddumpDomNode(DOM_Node& node);
 
extern char* XMLStrToStr(const XMLCh* xmlStr);
extern String XMLStrToString(const  XMLCh* xmlStr); 
extern XMLCh* XMLStr(const char* string);
extern bool   isXMLStrSame(const XMLCh* xmlStr1, const XMLCh* xmlStr2);

// eraxxon: returns a string of length 0 if 'i' or 'attr' refers to a
// non-present (or invalid) attribute.
extern String getAttr(AttributeList& attributes, int i); 
extern String getAttr(AttributeList& attributes, const XMLCh* const attr); 

extern void ReportException(const char* msg, const XMLException& toCatch);
extern void ReportException(const char* msg, const SAXException& toCatch);

void     outputContent(std::ostream& target, const DOMString &s, 
		       bool doEscapes);

std::ostream& operator<<(std::ostream& target, const DOMString& toWrite);
std::ostream& operator<<(std::ostream& target, DOM_Node& toWrite);

class SAXHandlerBase : public HandlerBase {
public: 
  SAXHandlerBase(bool verbose); 
  // verbose => prints errors/fatalErrors 

  // report to cerr and throw exception 
  virtual void error(const SAXParseException& e); 
  virtual void fatalError(const SAXParseException& e); 
  virtual void warning(const SAXParseException& e); 

  int LastErrorLine() const { return lastErrorLine; }
  int LastWarningLine() const { return lastWarningLine; }
  int LastErrorCol() const { return lastErrorCol; }
  int LastWarningCol() const { return lastWarningCol; }
  
private: 
  bool verbose; 
  int lastErrorLine; 
  int lastWarningLine; 
  int lastErrorCol; 
  int lastWarningCol; 
}; 

class DOMTreeErrorReporter : public ErrorHandler
{
public:
  DOMTreeErrorReporter(String &userFile, String &tmpFile, int tmpPrefixLines); 
    
  // -----------------------------------------------------------------------
  //  Constructors and Destructor
  // -----------------------------------------------------------------------
  DOMTreeErrorReporter() {} 
  ~DOMTreeErrorReporter() {} 

  // -----------------------------------------------------------------------
  //  Implementation of the error handler interface
  // -----------------------------------------------------------------------
  void warning(const SAXParseException& toCatch);
  void error(const SAXParseException& toCatch);
  void fatalError(const SAXParseException& toCatch);
  void resetErrors();

private:
  String realUserFile;
  String tmpUserFile;
  int tmpPrefixLines;
};

#endif
