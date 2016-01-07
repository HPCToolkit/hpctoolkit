// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
//   $HeadURL$
//
// Purpose:
//   XML adaptor for the program structure file (PGM)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef profxml_PGMDocHandler
#define profxml_PGMDocHandler

//************************ System Include Files ******************************

#include <string>
#include <map>

//************************* User Include Files *******************************

#include "XercesSAX2.hpp"
#include "DocHandlerArgs.hpp"

#include <lib/prof/Struct-Tree.hpp>

#include <lib/support/PointerStack.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

class PGMDocHandler : public XERCES_CPP_NAMESPACE::DefaultHandler {
public:
  enum Doc_t { Doc_NULL, Doc_STRUCT, Doc_GROUP };
  static const char* ToString(Doc_t docty);

private:
    std::map<std::string, Prof::Struct::Proc*> idToProcMap;

public:

  PGMDocHandler(Doc_t ty, Prof::Struct::Tree* structure,
		DocHandlerArgs& args);
  ~PGMDocHandler();

  void
  startElement(const XMLCh* const uri, const XMLCh* const name,
	       const XMLCh* const qname,
	       const XERCES_CPP_NAMESPACE::Attributes& attributes);
  void
  endElement(const XMLCh* const uri, const XMLCh* const name,
	     const XMLCh* const qname);


  void
  getLineAttr(SrcFile::ln& begLn, SrcFile::ln& endLn,
	      const XERCES_CPP_NAMESPACE::Attributes& attributes);

  //--------------------------------------
  // SAX2 error handler interface
  //--------------------------------------
  void
  error(const SAXParseException& e);

  void
  fatalError(const SAXParseException& e);

  void
  warning(const SAXParseException& e);

private:

  class StackEntry_t {
  public:
    StackEntry_t(Prof::Struct::ANode* entry_ = NULL,
		 Prof::Struct::ANode* shadow_ = NULL)
      : entry(entry_), shadow(shadow_), isLeaf(true) { }
    ~StackEntry_t() { }
    
    Prof::Struct::ANode*
    getScope() const
    { return entry; }

    Prof::Struct::ANode*
    GetShadow() const
    { return shadow; }

    void
    SetScope(Prof::Struct::ANode* x)
    { entry = x; }

    void
    SetShadow(Prof::Struct::ANode* x)
    { shadow = x; }

    // Is this a leaf node?
    bool
    IsLeaf()
    { return isLeaf; }
    
    void
    SetLeaf(bool x)
    { isLeaf = x; }

    bool
    IsGroupScope()
    { return (entry->type() == Prof::Struct::ANode::TyGroup); }

    bool
    IsNonGroupLeaf()
    { return (IsLeaf() && !IsGroupScope()); }

  private:
    Prof::Struct::ANode* entry;
    Prof::Struct::ANode* shadow;
    bool isLeaf;
  };
  
  
  StackEntry_t*
  getStackEntry(unsigned int idx)
  { return static_cast<StackEntry_t*>(scopeStack.Get(idx)); }
  
  Prof::Struct::ANode*
  getCurrentScope()
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Top());
    return entry->getScope();
  }

  Prof::Struct::ANode*
  getScope(unsigned int idx)
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Get(idx));
    return entry->getScope();
  }

  Prof::Struct::ANode*
  getShadowScope(unsigned int idx)
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Get(idx));
    return entry->GetShadow();
  }

  void
  pushCurrentScope(Prof::Struct::ANode* scope)
  {
    StackEntry_t* entry = new StackEntry_t(scope);
    scopeStack.Push(entry);
  }
  
  void
  popCurrentScope()
  {
    StackEntry_t* entry = static_cast<StackEntry_t*>(scopeStack.Pop());
    delete entry;
  }
  
  // Find the current file scope (include top of stack)
  Prof::Struct::File*
  findCurrentFile();

  // Find the enclosing GroupScope stack depth (excluding the top of
  // stack) or return 0 if not found.
  unsigned int
  findEnclosingGroupScopeDepth();


  void
  processGroupDocEndTag();
  
private:
  Doc_t m_docty;
  DocHandlerArgs& m_args;
  Prof::Struct::Tree* m_structure;
  
  // variables for constant values during file processing
  double m_version;     // initialized to a negative

  // current static context
  Prof::Struct::Root* m_curRoot;
  Prof::Struct::LM*   m_curLM;
  Prof::Struct::File* m_curFile;
  Prof::Struct::Proc* m_curProc;

  unsigned groupNestingLvl;

  // stack of StackEntry_t representing current scope context.  Top of
  // stack is most deeply nested scope.
  PointerStack scopeStack;

private:
  // Note: these cannot be static since the Xerces must be initialized
  // first.

  // element names
  const XMLCh *const elemStructure;
  const XMLCh *const elemLM;
  const XMLCh *const elemFile;
  const XMLCh *const elemProc;
  const XMLCh *const elemAlien;
  const XMLCh *const elemLoop;
  const XMLCh *const elemStmt;
  const XMLCh *const elemGroup;

  // attribute names
  const XMLCh *const attrVer;
  const XMLCh *const attrId;
  const XMLCh *const attrName;
  const XMLCh *const attrFile;
  const XMLCh *const attrLnName;
  const XMLCh *const attrLine;
  const XMLCh *const attrVMA;
};


//****************************************************************************

#define PGM_Throw(streamArgs) DIAG_ThrowX(PGMException, streamArgs)

class PGMException : public Diagnostics::Exception {
public:
  PGMException(const std::string x,
	       const char* filenm = NULL, unsigned int lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
  { }

  virtual std::string
  message() const
  { return "HPCToolkitStructure file error [STRUCTException]: " + what(); }

private:
};

#endif  // profxml_PGMDocHandler

