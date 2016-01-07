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
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Struct_LocationMgr_hpp
#define Struct_LocationMgr_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>

#include <deque>
#include <map>

#include <typeinfo>

#include <cctype>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/isa/ISATypes.hpp>
#include <lib/prof/Struct-Tree.hpp>
#include <lib/support/ProcNameMgr.hpp>

//*************************** Forward Declarations **************************

namespace BAnal {
namespace Struct {

  inline bool
  ctxtNameEqFuzzy(const string& ctxtnm, const string& x)
  {
    // perform a 'fuzzy' match: e.g., 'm_procnm' name can be 'GetLB'
    // even though the full context name is Array2D<double>::GetLB(int).
    size_t pos = ctxtnm.find(x);
    return (pos != string::npos && (pos == 0 || std::ispunct(ctxtnm[pos-1])));
  }

  inline bool
  ctxtNameEqFuzzy(const Prof::Struct::ACodeNode* callCtxt, const string& x)
  {
    bool t1 = ctxtNameEqFuzzy(callCtxt->name(), x);
    bool t2 = false;
    if (typeid(*callCtxt) == typeid(Prof::Struct::Proc)) {
      const Prof::Struct::Proc* pStrct =
	dynamic_cast<const Prof::Struct::Proc*>(callCtxt);
      t2 = ctxtNameEqFuzzy(pStrct->linkName(), x);
    }
    return (t1 || t2);
  }


} // namespace Struct
} // namespace BAnal


//***************************************************************************
// LocationMgr
//***************************************************************************

namespace BAnal {

namespace Struct {

// --------------------------------------------------------------------------
// 'LocationMgr' manages the location of loops and statements by
// using either the original context or creating appropriate alien
// contexts (e.g. for inlining).
// --------------------------------------------------------------------------

class LocationMgr {
public:
  LocationMgr(Prof::Struct::LM* lm = NULL);
  ~LocationMgr();

  // set structure tree, debug mode, etc.
  void
  init(Prof::Struct::LM* lm = NULL);
  
  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  // begSeq: Given a Struct::Proc 'enclosingProc', prepare for a sequence
  //   of method invocations from the following list.  (The internal
  //   context stack should be empty.)
  void
  begSeq(Prof::Struct::Proc* enclosingProc, bool isFwdSubst = false);

  // locate: Given a parentless Struct::Loop 'loop', the original
  //   enclosing scope 'proposed_scope' and best-guess source-line
  //   information, delegate the task of locating 'loop', i.e., finding
  //   the source line enclosing scope.  Sets the current context to use
  //   the newly located loop.
  //
  // We assume that
  //   1) 'loop' was originally contained within 'proposed_scope', but
  //      may need to be relocated to a more appropriate scope.
  //   2) 'proposed_scope' itself has already been relocated, if necessary.
  void
  locate(Prof::Struct::Loop* loop,
	 Prof::Struct::ACodeNode* proposed_scope,
	 std::string& filenm, std::string& procnm, SrcFile::ln line, uint targetScopeID,
	 ProcNameMgr *procNmMgr);
  
  // locate: Given a parentless Struct::Stmt 'stmt', the original
  //   enclosing scope 'proposed_scope' and best-guess source-line
  //   information, delegate the task of locating 'stmt', i.e., finding
  //   the source line enclosing scope.  Sets the current context to use
  //   the newly located loop.
  //  
  // The above assumptions apply.
  void
  locate(Prof::Struct::Stmt* stmt,
	 Prof::Struct::ACodeNode* proposed_scope,
	 std::string& filenm, std::string& procnm, SrcFile::ln line, uint targetScopeID,
	 ProcNameMgr *procNmMgr);

  // endSeq: 
  void endSeq();
  
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  Prof::Struct::ACodeNode*
  curScope() const
  { return (m_ctxtStack.empty()) ? NULL : m_ctxtStack.front().scope(); }
  
  bool
  isParentScope(Prof::Struct::ACodeNode* scope) const
  { return (findCtxt(scope) != NULL); }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // Assume we know that file names match
  static bool
  containsLineFzy(Prof::Struct::ACodeNode* x, SrcFile::ln line,
		  bool loopIsAlien = false);

  static bool
  containsIntervalFzy(Prof::Struct::ACodeNode* x,
		      SrcFile::ln begLn, SrcFile::ln endLn);

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string
  toString(int flags = 0) const;

  // flags = -1: compressed dump / 0: normal dump / 1: extra info
  std::ostream&
  dump(std::ostream& os, int flags = 0) const;

  void
  ddump(int flags = 0) const;

  void
  debug(int x);

private:

  // -------------------------------------------------------
  // a context stack representing nested inlining
  // -------------------------------------------------------
  class Ctxt
  {
  public:
    Ctxt(Prof::Struct::ACodeNode* ctxt = NULL,
	 Prof::Struct::Loop* loop = NULL)
      : m_ctxt(ctxt), m_loop(loop),
	m_filenm( (isAlien()) ?
		    static_cast<Prof::Struct::Alien*>(ctxt)->fileName()
		  : static_cast<Prof::Struct::Proc*>(ctxt)->ancestorFile()->name() )
    { }
    
    ~Ctxt() { }

    // enclosing (calling) context (Struct::Proc, Struct::Alien)
    Prof::Struct::ACodeNode*
    ctxt() const
    { return m_ctxt; }
    
    // enclosing file name
    const std::string&
    fileName() const
    { return m_filenm; }

    // current enclosing loop
    Prof::Struct::Loop*
    loop() const
    { return m_loop; }

    Prof::Struct::Loop*&
    loop()
    { return m_loop; }
    
    // current enclosing scope
    Prof::Struct::ACodeNode*
    scope() const
    {
      if (m_loop) {
	return m_loop;
      }
      else {
	return m_ctxt;
      }
    }

    // containsLine: given a line and possibly a file name, returns
    // whether the current context contains the line.  In the case
    // where a file name is available *and* matches the context, the
    // line matching is more lenient.
    bool
    containsLine(const string& filenm, SrcFile::ln line) const;

    bool
    containsLine(SrcFile::ln line) const;
    
    bool
    isAlien() const
    { return (typeid(*m_ctxt) == typeid(Prof::Struct::Alien)); }
    
    int
    level() const
    { return m_level; }

    int&
    level()
    { return m_level; }

    // debugging:
    //   flags: -1: tight dump / 0: normal dump
    std::string
    toString(int flags = 0, const char* pre = "") const;

    std::ostream&
    dump(std::ostream& os, int flags = 0, const char* pre = "") const;

    void
    ddump() const;

  private:
    Prof::Struct::ACodeNode*  m_ctxt;
    Prof::Struct::Loop* m_loop;
    const std::string& m_filenm;
    int m_level;
  };
  
  typedef std::list<Ctxt> MyStack;
  
private:
  
  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  enum CtxtChange_t {
    CtxtChange_MASK                = 0x000000ff,
    CtxtChange_NULL                = 0x00000000,
    
    CtxtChange_SAME                = 0x00000010,
    CtxtChange_POP                 = 0x00000020,
    CtxtChange_PUSH                = 0x00000040,

    // flags
    CtxtChange_FLAG_MASK           = 0xf0000000,
    CtxtChange_FLAG_NULL           = 0x00000000,
    CtxtChange_FLAG_RESTORE        = 0x10000000,
    CtxtChange_FLAG_REVERT         = 0x20000000,
    CtxtChange_FLAG_FIX_SCOPES     = 0x40000000
  };

  static bool
  CtxtChange_eq(CtxtChange_t x, CtxtChange_t y)
  {
    return ((CtxtChange_MASK & x) == (CtxtChange_MASK & y));
  }

  static void
  CtxtChange_set(CtxtChange_t& x, CtxtChange_t c)
  {
    uint t = (uint)x;
    t &= CtxtChange_FLAG_MASK; // reset
    t |= (uint)c;
    x = (CtxtChange_t)t;
  }


  static bool
  CtxtChange_isFlag(CtxtChange_t x, CtxtChange_t flag)
  {
    return ((CtxtChange_FLAG_MASK & x) == flag);
  }

  static void
  CtxtChange_setFlag(CtxtChange_t& x, CtxtChange_t flag)
  {
    uint t = (uint)x;
    t |= (uint)flag;
    x = (CtxtChange_t)t;
  }

  static std::string
  toString(CtxtChange_t x);

  // determineContext: Given the proposed enclosing scope
  // 'proposed_scope' and the best-guess source file, procedure and
  // line for some entity, determine where that entity is best located
  // w.r.t. the current context stack and prepare the stack
  // accordingly (potentially creating alien contexts).
  // 
  // Assume that 'proposed_scope' has already been located and thus
  // may already live within an alien context)
  CtxtChange_t
  determineContext(Prof::Struct::ACodeNode* proposed_scope,
		   std::string& filenm, std::string& procnm, SrcFile::ln line,
		   VMA begVMA, Prof::Struct::ACodeNode* loop, uint targetScopeID,
	           ProcNameMgr *procNmMgr);
  
  // fixCtxtStack: Yuck.
  void
  fixContextStack(const Prof::Struct::ACodeNode* proposed_scope);

  // fixScopeTree: Given a scope 'from_scope' that should be
  // located within the calling context 'true_ctxt' but specifically
  // within lines 'begLn' and 'endLn', perform the transformations.
  void
  fixScopeTree(Prof::Struct::ACodeNode* from_scope,
	       Prof::Struct::ACodeNode* true_ctxt,
	       SrcFile::ln begLn, SrcFile::ln endLn, uint targetScopeID);

  // alienateScopeTree: place all non-Alien children of 'scope' into an Alien
  // context based on 'alien' except for 'exclude'
  void
  alienateScopeTree(Prof::Struct::ACodeNode* scope,
		    Prof::Struct::Alien* alien,
		    Prof::Struct::ACodeNode* exclude, uint targetScopeID);

  
  // revertToLoop: Pop contexts between top and 'ctxt' until
  // 'ctxt->loop()' is the deepest loop on the stack.  (If
  // ctxt->loop() is NULL then there should be no deeper loop.) We
  // chop off any loops deeper than ctxt.
  int
  revertToLoop(Ctxt* ctxt);
  
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  Ctxt*
  topCtxt() const
  {
    return ( m_ctxtStack.empty() ?
	     NULL : const_cast<Ctxt*>(&(m_ctxtStack.front())) );
  }

  Ctxt&
  topCtxtRef() const
  { return const_cast<Ctxt&>(m_ctxtStack.front()); }

  void
  pushCtxt(const Ctxt& ctxt)
  {
    int nxt_lvl = (m_ctxtStack.empty()) ? 1 : topCtxtRef().level() + 1;
    m_ctxtStack.push_front(ctxt);
    topCtxtRef().level() = nxt_lvl;
  }


  // -------------------------------------------------------
  // findCtxt
  // -------------------------------------------------------

  // switch_findCtxt: calls the appriate version of 'findCtxt'
  // based on what information is available, i.e. it is a findCtxt
  // "switch" statement.
  Ctxt*
  switch_findCtxt(const string& filenm, const string& procnm,
		  SrcFile::ln line, const Ctxt* base_ctxt = NULL) const;
  

  // findCtxt: find the the "first" instance of ctxt in the stack
  // matching against ctxt() (not scope()) and not going past 'base'.
  // More specifically, we find the first non-alien context before
  // 'base' and if that does not exist, the deepest alien context
  // before 'base'.
  
  class FindCtxt_MatchOp
  {
  public:
    virtual
    ~FindCtxt_MatchOp()
    { }

    virtual bool
    operator()(const Ctxt& ctxt) const = 0;
  };


  class FindCtxt_MatchFPLOp 
    : public FindCtxt_MatchOp
  {
  public:
    FindCtxt_MatchFPLOp(const string& filenm, const string& procnm,
			SrcFile::ln line)
      : m_filenm(filenm), m_procnm(procnm), m_line(line)
    { }
    
    virtual
    ~FindCtxt_MatchFPLOp()
    { }
    
    virtual bool
    operator()(const Ctxt& ctxt) const
    {
      return (ctxt.containsLine(m_filenm, m_line)
	      && ctxtNameEqFuzzy(ctxt.ctxt(), m_procnm));
    }

  private:
    const string& m_filenm, m_procnm;
    SrcFile::ln m_line;
  };


  class FindCtxt_MatchFPOp
    : public FindCtxt_MatchOp
  {
  public:
    FindCtxt_MatchFPOp(const string& filenm, const string& procnm)
      : m_filenm(filenm), m_procnm(procnm)
    { }

    virtual
    ~FindCtxt_MatchFPOp()
    { }
    
    virtual bool
    operator()(const Ctxt& ctxt) const
    {
      return (ctxt.fileName() == m_filenm && ctxt.ctxt()->name() == m_procnm);
    }

  private:
    const string& m_filenm, m_procnm;
  };


  class FindCtxt_MatchFLOp
    : public FindCtxt_MatchOp
  {
  public:
    FindCtxt_MatchFLOp(const string& filenm, SrcFile::ln line)
      : m_filenm(filenm), m_line(line)
    { }

    virtual
    ~FindCtxt_MatchFLOp()
    { }
    
    virtual bool
    operator()(const Ctxt& ctxt) const
    {
      return (ctxt.containsLine(m_filenm, m_line));
    }

  private:
    const string& m_filenm;
    SrcFile::ln m_line;
  };


  class FindCtxt_MatchFOp : public FindCtxt_MatchOp {
  public:
    FindCtxt_MatchFOp(const string& filenm)
      : m_filenm(filenm)
    { }

    virtual ~FindCtxt_MatchFOp()
    { }
    
    virtual bool
    operator()(const Ctxt& ctxt) const
    {
      return (ctxt.fileName() == m_filenm);
    }

  private:
    const string& m_filenm;
  };


  class FindCtxt_MatchLOp : public FindCtxt_MatchOp {
  public:
    FindCtxt_MatchLOp(SrcFile::ln line)
      : m_line(line)
    { }

    virtual
    ~FindCtxt_MatchLOp()
    { }
    
    virtual bool
    operator()(const Ctxt& ctxt) const
    {
      return (ctxt.containsLine(m_line));
    }

  private:
    SrcFile::ln m_line;
  };


  Ctxt*
  findCtxt(Prof::Struct::ACodeNode* ctxt) const;

  Ctxt*
  findCtxt(FindCtxt_MatchOp& op, const Ctxt* base = NULL) const;
  
  Ctxt*
  findCtxt(const string& filenm, const string& procnm, SrcFile::ln line,
	   const Ctxt* base = NULL) const
  { FindCtxt_MatchFPLOp op(filenm, procnm, line); return findCtxt(op, base); }

  Ctxt*
  findCtxt(const string& filenm, const string& procnm,
	   const Ctxt* base = NULL) const
  { FindCtxt_MatchFPOp op(filenm, procnm); return findCtxt(op, base); }

  Ctxt*
  findCtxt(const std::string& filenm, SrcFile::ln line,
	   const Ctxt* base = NULL) const
  { FindCtxt_MatchFLOp op(filenm, line); return findCtxt(op, base); }

  Ctxt*
  findCtxt(const std::string& filenm, const Ctxt* base = NULL) const
  { FindCtxt_MatchFOp op(filenm); return findCtxt(op, base); }

  Ctxt*
  findCtxt(SrcFile::ln line, const Ctxt* base = NULL) const
  { FindCtxt_MatchLOp op(line); return findCtxt(op, base); }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  
  // AlienStrctMapKey: Note that we do not include line numbers
  // because these will be changing.
  struct AlienStrctMapKey
  {
    AlienStrctMapKey(Prof::Struct::ACodeNode* x,
		     const std::string& y, const std::string& z)
      : parent_scope(x), filenm(y), procnm(z)
    { }

    Prof::Struct::ACodeNode* parent_scope; // the first enclosing LOOP or PROC scope
    std::string filenm;     // file name
    std::string procnm;     // procedure name
  };
  
  struct lt_AlienStrctMapKey
  {
    // return true if x1 < x2; false otherwise
    bool operator()(const AlienStrctMapKey& x1,
		    const AlienStrctMapKey& x2) const
    {
      if (x1.parent_scope == x2.parent_scope) {
	int filecmp = x1.filenm.compare(x2.filenm);
	if (filecmp == 0) {
	  return (x1.procnm.compare(x2.procnm) < 0);
	}
	else {
	  return (filecmp < 0);
	}
      }
      else {
	return (x1.parent_scope < x2.parent_scope);
      }
    }
  };

  typedef std::multimap<AlienStrctMapKey,
			Prof::Struct::Alien*,
			lt_AlienStrctMapKey> AlienStrctMap;

  // Find or create an Struct::Alien
  Prof::Struct::Alien*
  demandAlienStrct(Prof::Struct::ACodeNode* parent_scope,
		   const std::string& filenm, const std::string& procnm,
		   const std::string& displaynm, SrcFile::ln line, uint targetScopeID);
  
public:
  // evict an alien from the m_alienMap
  void
  evictAlien(Prof::Struct::ACodeNode* parent_scope, 
	     Prof::Struct::Alien* alien);

private:
  MyStack     m_ctxtStack; // cf. topCtxt() [begin()/front() is the top]
  Prof::Struct::LM* m_loadMod;
  int         mDBG;
  bool        m_isFwdSubst;

  AlienStrctMap m_alienMap;
};


} // namespace Struct

} // namespace BAnal

//***************************************************************************

#endif // Struct_LocationMgr_hpp
