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

//************************* User Include Files *******************************

#include "XMLAdapter.h"
#include "Driver.h"
#include <lib/support/String.h>

//************************ Forward Declarations ******************************

class NodeRetriever; 
class ProcScope; 

//****************************************************************************

class PROFILEDocHandler : public SAXHandlerBase {
public:

  PROFILEDocHandler(NodeRetriever* const retriever, Driver* _driver);
  ~PROFILEDocHandler();

  // Must be called before parsing starts.  Notifies the handler
  // which metric data to extract from the profile. 
  void Initialize(int metricIndx, const char* profileFileName);

  // overridden functions
  void startElement(const XMLCh* const name, AttributeList& attributes);
  void endElement(const XMLCh* const name);

private:
  NodeRetriever* nodeRetriever;
  Driver* driver;

  // variables for constant values during file processing
  String profileFile;
  int metricPerfDataTblIndx; // index into PerfData table
  String metricName; // metric name; must match with a name in 'profileFile'
  String metricNameShort; // a short version of the metric name 
  double profVersion;     // initialized to a negative
  
  // variables for transient values during file processing
  String lmName;
  String srcFileName;
  String funcName;
  ProcScope *funcScope;
  int line;
};

class PROFILEException {
public:
  PROFILEException (String msg) {
    msgtext = msg;
  }
  String message() const { 
    return msgtext; 
  }
private:
  String msgtext;
};

#endif  // PROFILEDocHandler_h
