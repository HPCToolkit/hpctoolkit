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
//    PGMDocHandler.h
//
// Purpose:
//    XML adaptor for the program structure file (PGM)
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef PGMDocHandler_H
#define PGMDocHandler_H

//************************ System Include Files ******************************

//************************* User Include Files *******************************

#include "XMLAdapter.h"
#include "Driver.h"
#include <lib/support/String.h>
#include <lib/support/PointerStack.h>

//************************ Forward Declarations ******************************

class NodeRetriever; 
class ProcScope; 
class CodeInfo;

//****************************************************************************

class PGMDocHandler : public SAXHandlerBase {
public:

  PGMDocHandler(NodeRetriever* const retriever, Driver* _driver);
  ~PGMDocHandler();

  // overridden functions
  void startElement(const XMLCh* const name, AttributeList& attributes);
  void endElement(const XMLCh* const name);
  
private:
  NodeRetriever* nodeRetriever;
  Driver* driver;
  
  // variables for constant values during file processing
  double pgmVersion;     // initialized to a negative

  // variables for transient values during file processing
  String lmName;
  String srcFileName; 
  String funcName; 
  ProcScope *funcScope; 
  PointerStack scopeStack; // the top is the scope where we must add stmts
  CodeInfo *currentScope;
};

class PGMException {
public:
  PGMException (String msg) {
    msgtext = msg;
  }
  String message() const { 
    return msgtext; 
  }
private:
  String msgtext;
};

#endif  // PGMDocHandler_H
