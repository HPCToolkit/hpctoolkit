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

#include <string>
#include <map>

//************************* User Include Files *******************************

#include "HPCViewSAX2.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/PointerStack.hpp>

//************************ Forward Declarations ******************************

class Driver;

class NodeRetriever; 

//****************************************************************************

class PGMDocHandler : public DefaultHandler {
public:
  enum Doc_t { Doc_NULL, Doc_STRUCT, Doc_GROUP };
  static const char* ToString(Doc_t docty);

public:

  PGMDocHandler(Doc_t ty, NodeRetriever* const retriever, Driver* _driver);
  ~PGMDocHandler();

  void startElement(const XMLCh* const uri, const XMLCh* const name, const XMLCh* const qname, const Attributes& attributes);
  void endElement(const XMLCh* const uri, const XMLCh* const name, const XMLCh* const qname);

  //--------------------------------------
  // SAX2 error handler interface
  //--------------------------------------
  void error(const SAXParseException& e);
  void fatalError(const SAXParseException& e);
  void warning(const SAXParseException& e);

private:

  class StackEntry_t {
  public:
    StackEntry_t(ScopeInfo* entry_ = NULL, ScopeInfo* shadow_ = NULL) 
      : entry(entry_), shadow(shadow_), isLeaf(true) { }
    ~StackEntry_t() { }
    
    ScopeInfo* GetScope() const { return entry; }
    ScopeInfo* GetShadow() const { return shadow; }

    void SetScope(ScopeInfo* x) { entry = x; }
    void SetShadow(ScopeInfo* x) { shadow = x; }

    // Is this a leaf node?
    bool IsLeaf() { return isLeaf; }
    void SetLeaf(bool x) { isLeaf = x; }

    bool IsGroupScope() { return (entry->Type() == ScopeInfo::GROUP); }
    bool IsNonGroupLeaf() { return (IsLeaf() && !IsGroupScope()); }

  private:
    ScopeInfo* entry;
    ScopeInfo* shadow;
    bool isLeaf;
  };
  
  
  StackEntry_t* GetStackEntry(unsigned int idx)
  {
    return static_cast<StackEntry_t*>(scopeStack.Get(idx));
  }
  
  ScopeInfo* GetCurrentScope()
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Top());
    return entry->GetScope();
  }

  ScopeInfo* GetScope(unsigned int idx)
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Get(idx));
    return entry->GetScope();
  }

  ScopeInfo* GetShadowScope(unsigned int idx)
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Get(idx));
    return entry->GetShadow();
  }

  void PushCurrentScope(ScopeInfo* scope)
  {
    StackEntry_t* entry = new StackEntry_t(scope);
    scopeStack.Push(entry);
  }
  
  void PopCurrentScope()
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Pop());
    delete entry;
  }
  
  // Find the current file scope (include top of stack)
  FileScope* FindCurrentFileScope();

  // Find the enclosing GroupScope stack depth (excluding the top of
  // stack) or return 0 if not found.
  unsigned int FindEnclosingGroupScopeDepth();


  void ProcessGroupDocEndTag();
  
private:
  Doc_t docty;
  NodeRetriever* nodeRetriever;
  Driver* driver;
  
  // variables for constant values during file processing
  double pgmVersion;     // initialized to a negative

  // variables for transient values during file processing
  std::string currentLmName;    // only one LM on the stack at a time
  std::string currentFileName;  // only one File on the stack at a time
  std::string currentFuncName;
  ProcScope* currentFuncScope;
  unsigned groupNestingLvl;

  // stack of StackEntry_t representing current scope context.  Top of
  // stack is most deeply nested scope.
  PointerStack scopeStack;

private:
  // Note: these cannot be static since the Xerces must be initialized
  // first.

  // element names
  const XMLCh *const elemPgm;
  const XMLCh *const elemLM; 
  const XMLCh *const elemFile; 
  const XMLCh *const elemProc; 
  const XMLCh *const elemLoop; 
  const XMLCh *const elemStmt;
  const XMLCh *const elemGroup; 
  const XMLCh *const elemAlien; 

  // attribute names
  const XMLCh *const attrVer; 
  const XMLCh *const attrName; 
  const XMLCh *const attrLnName; 
  const XMLCh *const attrBegin; 
  const XMLCh *const attrEnd; 
  const XMLCh *const attrVMA;
};

//****************************************************************************

class PGMException : public Diagnostics::Exception {
public:
  PGMException(const std::string x,
	       const char* filenm = NULL, unsigned int lineno = 0) 
    : Diagnostics::Exception(x, filenm, lineno)
  { }

  virtual std::string message() const { 
    return "PGM file error [PGMException]: " + what();
  }

private:
};

#endif  // PGMDocHandler_H
