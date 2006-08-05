// -*-C++-*-
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

//***************************************************************************
//
// File:
//    PROFILEDocHandler.h
//
// Purpose:
//    XML adaptor for the profile data file (PROFILE)
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef PROFILEDocHandler_h
#define PROFILEDocHandler_h

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

//************************* Xerces Declarations ******************************

#include "HPCViewSAX2.hpp"

//************************ Forward Declarations ******************************

class NodeRetriever; 
class ProcScope; 
class FileScope; 
class LoadModScope; 
class Driver; 

//****************************************************************************

class PROFILEDocHandler : public DefaultHandler {
public:

  PROFILEDocHandler(NodeRetriever* const retriever, Driver* _driver);
  ~PROFILEDocHandler();

  // Must be called before parsing starts.  Notifies the handler
  // which metric data to extract from the profile. 
  void Initialize(int metricIndx, const char* profileFileName);
  void Initialize(int metricIndx, const std::string& profileFileName)
    { Initialize(metricIndx, profileFileName.c_str()); }

  // overridden functions
  void startElement(const XMLCh* const uri, const XMLCh* const name, const XMLCh* const qname, const Attributes& attributes);
  void endElement(const XMLCh* const uri, const XMLCh* const name, const XMLCh* const qname);

  //--------------------------------------
  // SAX2 error handler interface
  //--------------------------------------
  void error(const SAXParseException& e);
  void fatalError(const SAXParseException& e);
  void warning(const SAXParseException& e);

private:
  NodeRetriever* nodeRetriever;
  Driver* driver;

  // variables for constant values during file processing
  std::string profileFile;
  int metricPerfDataTblIndx; // index into PerfData table
  std::string metricName; // metric name; must match with a name in 'profileFile'
  std::string metricNameShort; // a short version of the metric name 
  double profVersion;     // initialized to a negative
  
  // variables for transient values during file processing
  std::string lmName;
  std::string srcFileName;
  std::string funcName;
  ProcScope *procScope;
  FileScope *fileScope;
  LoadModScope *lmScope;
  int line;

private:
  const XMLCh *const elemProfile;
  const XMLCh *const elemProfileHdr;
  const XMLCh *const elemProfileParams;
  const XMLCh *const elemTarget;
  const XMLCh *const elemMetrics;
  const XMLCh *const elemMetricDef;

  const XMLCh *const elemProfileScopeTree;
  const XMLCh *const elemPgm;
  const XMLCh *const elemGroup;
  const XMLCh *const elemLM;
  const XMLCh *const elemFile;
  const XMLCh *const elemProc;
  const XMLCh *const elemLoop;
  const XMLCh *const elemStmt;
  const XMLCh *const elemMetric;

  // attribute names for PROFILE elements
  const XMLCh *const attrVer;
  const XMLCh *const attrShortName;
  const XMLCh *const attrNativeName;
  const XMLCh *const attrPeriod;
  const XMLCh *const attrUnits;
  const XMLCh *const attrDisplayName;
  const XMLCh *const attrDisplay;

  // attribute names for PGM elements
  const XMLCh *const attrName;
  const XMLCh *const attrLnName;
  const XMLCh *const attrBegin;
  const XMLCh *const attrEnd;
  const XMLCh *const attrId;
  const XMLCh *const attrVal;

};

class PROFILEException {
public:
  PROFILEException (const std::string& msg) {
    msgtext = msg;
  }
  std::string message() const { 
    return msgtext; 
  }
private:
  std::string msgtext;
};

#endif  // PROFILEDocHandler_h
