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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <typeinfo>

#include <string>
using std::string;

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <sstream>

#include <utility>
using std::pair;

#include <climits>

//*************************** User Include Files ****************************

#include "bloop_LocationMgr.hpp"

#include <lib/binutils/BinUtils.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations **************************

#define DBG (0 && mDBG)

//*************************** Forward Declarations **************************

static const string RELOCATED = "[reloc-from]";

inline bool 
RELOCATEDcmp(const char* x)
{
  return (strncmp(x, RELOCATED.c_str(), RELOCATED.length()) == 0);
}

inline bool 
RELOCATEDcmp(const string& x)
{
  return RELOCATEDcmp(x.c_str());
}

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// LocationMgr
//***************************************************************************

namespace banal {

namespace bloop {

LocationMgr::LocationMgr(LoadModScope* lm)
{
  init(lm);
}


LocationMgr::~LocationMgr()
{
}


void
LocationMgr::init(LoadModScope* lm)
{
  m_loadMod = lm;
  mDBG = 0;
}


void
LocationMgr::begSeq(ProcScope* enclosingProc)
{
  DIAG_Assert(m_ctxtStack.empty() && m_alienMap.empty(), 
	      "LocationMgr contains leftover crud!");
  pushCtxt(Ctxt(enclosingProc));
}


void
LocationMgr::locate(LoopScope* loop, CodeInfo* proposed_scope,
		    string& filenm, string& procnm, suint line)
{
  DIAG_DevMsgIf(DBG, "LocationMgr::locate: " << loop->toXML() << "\n"
		<< "  proposed: " << proposed_scope->toXML() << "\n"
		<< "  guess: {" << filenm << "}[" << procnm << "]:" << line);
  determineContext(proposed_scope, filenm, procnm, line);
  Ctxt& encl_ctxt = m_ctxtStack.front();
  loop->LinkAndSetLineRange(encl_ctxt.scope());
}


void
LocationMgr::locate(StmtRangeScope* stmt, CodeInfo* proposed_scope,
		    string& filenm, string& procnm, suint line)
{
  DIAG_DevMsgIf(DBG, "LocationMgr::locate: " << stmt->toXML() << "\n"
		<< "  proposed: " << proposed_scope->toXML() << "\n"
		<< "  guess: {" << filenm << "}[" << procnm << "]:" << line);
  // FIXME (minor): manage stmt cache! if stmt already exists, only add vma
  determineContext(proposed_scope, filenm, procnm, line);
  Ctxt& encl_ctxt = m_ctxtStack.front();
  stmt->LinkAndSetLineRange(encl_ctxt.scope());
}


void
LocationMgr::endSeq()
{
  m_ctxtStack.clear();
  m_alienMap.clear();
}


bool
LocationMgr::containsLineFzy(CodeInfo* x, suint line, bool loopIsAlien)
{
  int beg_epsilon = 0, end_epsilon = 0;
  
  switch (x->Type()) {
    // procedure begin lines are very accurate (with debug info)
    // procedure end lines are not very accurate
    // loop begin lines are somewhat accurate
    // loop end line are not very accurate
    case ScopeInfo::PROC:  beg_epsilon = 5;  end_epsilon = 30;      break;
    case ScopeInfo::ALIEN: beg_epsilon = 10; end_epsilon = INT_MAX; break;
    case ScopeInfo::LOOP:  beg_epsilon = 5;  end_epsilon = INT_MAX;
                           if (loopIsAlien) { end_epsilon = 20; }   break;
    default: break;
  }
  
  return x->containsLine(line, beg_epsilon, end_epsilon);
}  


bool
LocationMgr::containsIntervalFzy(CodeInfo* x, suint begLn, suint endLn)
{
  int beg_epsilon = 0, end_epsilon = 0;
  
  switch (x->Type()) {
    // see assumptions above.
    case ScopeInfo::PROC:  beg_epsilon = 5;  end_epsilon = 10; break;
    case ScopeInfo::ALIEN: beg_epsilon = 10; end_epsilon = 10; break;
    case ScopeInfo::LOOP:  beg_epsilon = 5;  end_epsilon = 5;
    default: break;
  }
  
  return x->containsInterval(begLn, endLn, beg_epsilon, end_epsilon);
}  


std::string
LocationMgr::toString(int flags) const
{
  std::ostringstream os;
  dump(os, flags);
  return os.str();
}


std::ostream&
LocationMgr::dump(std::ostream& os, int flags) const
{
  os << "{ bloop::LocationMgr: Context Stack: [top ... bottom]\n";
  for (MyStack::const_iterator it = m_ctxtStack.begin();
       it != m_ctxtStack.end(); ++it) {
    it->dump(os, 0, "  ");
  }
  if (flags >= 1) {
    os << " --------------------------------------\n";
    for (AlienScopeMap::const_iterator it = m_alienMap.begin();
	 (it != m_alienMap.end()); ++it) {
      const AlienScopeMapKey& key = it->first;
      const AlienScope* a = it->second;
      os << "  " << hex << key.parent_scope << dec << " <" << key.filenm 
	 << ">[" << key.procnm << "] --> " << a->toXML() << endl;
    }
  }
  os << "}\n";
  os.flush();
  return os;
}


void
LocationMgr::ddump(int flags) const
{
  dump(std::cerr, flags);
}


void
LocationMgr::debug(int x)
{
  mDBG = x;
}

} // namespace bloop

} // namespace banal


//***************************************************************************


namespace banal {

namespace bloop {

bool 
LocationMgr::Ctxt::containsLine(suint line) const 
{
  if (isAlien()) {
    return (ctxt()->begLine() <= line); // FIXME (we don't know about file...)
  }
  else {
    return (ctxt()->containsLine(line));
  }
}


std::string
LocationMgr::Ctxt::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


std::ostream&
LocationMgr::Ctxt::dump(std::ostream& os, int flags, const char* pre) const
{
  os << pre << "{ file: " << fileName() << "\n";
  os << pre << "  ctxt: " << hex << ctxt() << dec 
     << ": " << ctxt()->toXML() << "\n";
  os << pre << "  loop: " << hex << loop() << dec;
  if (loop()) { os << ": " << loop()->toXML(); }
  os << " }";
  if (flags >= 0) {
    os << "\n";
  }
  os.flush();
  return os;
}


void
LocationMgr::Ctxt::ddump() const
{
  dump(std::cerr);
}


string
LocationMgr::toString(CtxtChange_t x)
{
  string str;
  
  if (x == CtxtChange_NULL) {
    str = "CtxtChange_NULL";
  }
  else {
    switch (CtxtChange_MASK & x) {
      case CtxtChange_SAME:              str = "CtxtChange_SAME"; break;
      case CtxtChange_POP  /*REVERT*/:   str = "CtxtChange_POP"; break;
      case CtxtChange_PUSH /*RELOCATE*/: str = "CtxtChange_PUSH"; break;
      default: DIAG_Die(DIAG_Unimplemented);
    }
  }

  if (CtxtChange_isFlag(x, CtxtChange_FLAG_RESTORE)) {
    str += "/RESTORE";
  }
  if (CtxtChange_isFlag(x, CtxtChange_FLAG_REVERT)) {
    str += "/REVERT";
  }
  
  return str;
}


LocationMgr::CtxtChange_t
LocationMgr::determineContext(CodeInfo* proposed_scope,
			      string& filenm, string& procnm, suint line)
{
  DIAG_DevMsgIf(DBG, "LocationMgr::determineContext");

  CtxtChange_t change = CtxtChange_NULL;
  
  // -----------------------------------------------------
  // Initialize context information
  // -----------------------------------------------------
  
  // proposed context file and procedure
  CodeInfo* proposed_ctxt_scope = dynamic_cast<CodeInfo*>(proposed_scope->Ancestor(ScopeInfo::PROC, ScopeInfo::ALIEN));
  Ctxt* proposed_ctxt = findCtxt(proposed_ctxt_scope);
  if (!proposed_ctxt) {
    // Restore context
    pushCtxt(Ctxt(proposed_ctxt_scope, NULL));
    proposed_ctxt = topCtxt();
    CtxtChange_setFlag(change, CtxtChange_FLAG_RESTORE);
  }
  const string& proposed_procnm = proposed_ctxt->ctxt()->name();
  const string& proposed_filenm = proposed_ctxt->fileName();
  LoopScope* proposed_loop = dynamic_cast<LoopScope*>(proposed_scope);
  
  // proposed loop lives within proposed context 
  proposed_ctxt->loop() = proposed_loop;
  
  // proposed_loop should be the most deeply nested loop on the stack
  int cnt = revertStack(proposed_ctxt);
  if (cnt) {
    CtxtChange_setFlag(change, CtxtChange_FLAG_REVERT);
  }
  
  
  // top context file and procedure name (alien if != to proposed_ctxt)
  const Ctxt* top_ctxt = topCtxt();
  const string& top_filenm = top_ctxt->fileName();
  const string& top_procnm = top_ctxt->ctxt()->name();

  DIAG_Assert(logic::implies(proposed_ctxt != top_ctxt, top_ctxt->isAlien()),
	      "Inconsistent context stack!");
  
  // -----------------------------------------------------
  // Attempt to find an existing context.  We first attempt to either
  // use the current top-of-stack or revert to an enclosing context.
  //
  // Notes:
  // - We cannot restore contexts beyond 'proposed_ctxt'.
  // - Assuming proposed_loop is non-NULL, the only time it is the
  //   immediate parent context is when 'proposed_ctxt' is at the
  //   top-of-the-stack.  Otherwise the stack has alien scopes (but no
  //   loop scopes) at the top.
  // -----------------------------------------------------
  const Ctxt* use_ctxt = switch_findCtxt(filenm, procnm, line, proposed_ctxt);

  // INVARIANT: ('use_ctxt' != NULL) ==> 'use_ctxt' is within the
  //   range ['top_ctxt', 'proposed_ctxt']

  DIAG_DevMsgIf(DBG, "  first ctxt:\n" 
		<< ((use_ctxt) ? use_ctxt->toString(-1, "  ") : ""));
  
  
  // -----------------------------------------------------
  // Setup what will be the current context
  // -----------------------------------------------------
  CtxtChange_set(change, CtxtChange_SAME);
  
  if (use_ctxt) {
    // Revert the stack to 'use_ctxt'
    if (use_ctxt != top_ctxt) {
      // pop contexts until we get to 'use_ctxt'
      while (top_ctxt != use_ctxt) {
	m_ctxtStack.pop_front();
	top_ctxt = topCtxt();
      }
      CtxtChange_set(change, CtxtChange_POP);

      // Possibly update use_context (if we had to short-circuit)
      use_ctxt = topCtxt();
    }
    
    // If our enclosing scope is a loop, then consult the loop bounds,
    // which we assume are approximately correct, though we place more
    // confidence in the begin line than the end line estimate.
    //
    // We consult the loop bounds for two reasons.  First, within an
    // alien context, we can't assume much about the context bounds
    // and the loop bounds enable us to detect inlining and forward
    // substitution.  Second, within a non-alien context we detect
    // inlining using the procedure bounds but can use the loop bounds
    // to detect forward substitution (which, e.g., may occur when
    // initializing the induction variable).  
    //
    // However, given the loop bound limitations, we only want to
    // relocate if the mismatch is significant because in many cases
    // the loop bounds can be improved by including what currently
    // appear to be 'nearby' statements but which are actually part of
    // the source loop.  For begin boundaries we can be fairly strict,
    // but want to allow loop initializations that usually have a
    // source line number slightly before the "head VMA".  For end
    // boundaries we have to be more lenient.  

    if (use_ctxt->loop()) {
      // INVARIANT: We must be in the proposed context.
      // INVARIANT: File names must match (or use_ctxt would be NULL)
      DIAG_Assert(use_ctxt == proposed_ctxt, "");
      if (IsValidLine(line) 
	  && !containsLineFzy(use_ctxt->loop(), line, use_ctxt->isAlien())) {
	use_ctxt = NULL; // force a relocation
      }
    }
  }
  
  if (!use_ctxt) {
    // Relocation! Add an alien context.
    DIAG_Assert(!filenm.empty(), "");
    CtxtChange_set(change, CtxtChange_PUSH);
    
    // Find or create the alien scope
    AlienScope* alien = 
      findOrCreateAlienScope(proposed_scope, filenm, procnm, line);
    pushCtxt(Ctxt(alien, NULL));
    use_ctxt = topCtxt();
  }

  DIAG_DevMsgIf(DBG, "  final ctxt [" << toString(change) << "]\n" 
		<< use_ctxt->toString(0, "  "));
  return change;
}


LocationMgr::Ctxt* 
LocationMgr::switch_findCtxt(const string& filenm, const string& procnm, 
			     suint line, 
			     const LocationMgr::Ctxt* base_ctxt) const
{
  // -----------------------------------------------------
  // Notes: 
  // - In general, assume that best-guess filename and line number are
  //   more reliable that procedure name.  In the common case,
  //   'top_procnm' is equal to 'procnm' (since inlined procedures
  //   usually assume the name of their context); therefore we assign
  //   more weight to file and line.
  // - Try to find the context using the 'best' information first;
  //   then use second best (instead searching in lockstep with best and
  //   second-best)
  // -----------------------------------------------------
  Ctxt* the_ctxt = NULL;

  if ( (filenm.empty() && procnm.empty()) || 
       (filenm.empty() && !procnm.empty()) ) {
    // If both filenm and procnm are NULL:
    //   1) 
    //   2) 
    if (IsValidLine(line)) {
      the_ctxt = findCtxt(line, base_ctxt);
      if (!the_ctxt) {
        the_ctxt = topCtxt(); // FIXME: relocate if line is out side??
      }
    }
    else {
      the_ctxt = topCtxt();
    }
  }
#if 0
  else if (filenm.empty() && !procnm.empty()) {
    // If only filenm is NULL:
    //   1) use proposed procedure file name if procnm matches it
    // FIXME: good enough for now: use proposed procedure file name
    the_ctxt = top_ctxt;
  }
#endif
  else if (!filenm.empty() && procnm.empty()) {
    if (IsValidLine(line)) {
      the_ctxt = findCtxt(filenm, line, base_ctxt);
    }
    else {
      the_ctxt = findCtxt(filenm, base_ctxt);
    }
  }
  else {
    // INVARIANT: neither filenm nor procnm are NULL (empty)
    if (IsValidLine(line)) {
      the_ctxt = findCtxt(filenm, procnm, line, base_ctxt);
      //if (!the_ctxt && procnm == proposed_procnm) {
      //the_ctxt = findCtxt(filenm, line, base_ctxt); // FIXME do we need this?
      //}
    }
    else {
      the_ctxt = findCtxt(filenm, base_ctxt);
    }
  }

  return the_ctxt;
}


LocationMgr::Ctxt*
LocationMgr::findCtxt(CodeInfo* ctxt_scope) const
{
  for (MyStack::const_iterator it = m_ctxtStack.begin();
       it != m_ctxtStack.end(); ++it) {
    const Ctxt& ctxt = *it;
    if (ctxt_scope == ctxt.ctxt()) {
      return const_cast<Ctxt*>(&ctxt);
    }
  }
  return NULL;
}


LocationMgr::Ctxt*
LocationMgr::findCtxt(FindCtxt_MatchOp& op, 
		      const LocationMgr::Ctxt* base) const
{
  const LocationMgr::Ctxt* foundAlien = NULL;
  
  for (MyStack::const_iterator it = m_ctxtStack.begin();
       it != m_ctxtStack.end(); ++it) {
    const Ctxt& ctxt = *it;
    if (op(ctxt)) {
      if (ctxt.isAlien()) {
	foundAlien = &ctxt;
      }
      else {
	return const_cast<Ctxt*>(&ctxt);
      }
    }
    if (base && base == &ctxt) {
      break;
    }
  }
  return const_cast<Ctxt*>(foundAlien);
}


int
LocationMgr::revertStack(LocationMgr::Ctxt* ctxt)
{
  // Find the range [beg, end) between the top of the stack and ctxt
  // that should be popped in order to ensure that 'ctxt' has the
  // deepest loop on the stack.  (If ctxt has no loop, then there
  // should still be no deeper loop.) We chop off any loops deeper
  // than ctxt.
  //
  // We are guaranteed that the stack is never empty
  MyStack::iterator beg = m_ctxtStack.begin();
  MyStack::iterator end = m_ctxtStack.begin();
  for (MyStack::iterator it = m_ctxtStack.begin(); &(*it) != ctxt; ++it) {
    Ctxt& x = *it; 
    if (x.loop() && x.loop() != ctxt->loop()) {
      end = it; end++;
    }
  }
  
  int count = 0;
  if (beg != end) {
    m_ctxtStack.erase(beg, end);
    count = 1;
  }

  return count;
}


AlienScope* 
LocationMgr::findOrCreateAlienScope(CodeInfo* parent_scope,
				    const std::string& filenm,
				    const std::string& procnm, suint line)
{
  // INVARIANT: 'parent_scope' should either be the top of the stack
  // or the first first enclosing LOOP or PROC of the top of the
  // stack.

  AlienScope* alien = NULL;
  AlienScopeMapKey key(parent_scope, filenm, procnm);

  pair<AlienScopeMap::const_iterator, AlienScopeMap::const_iterator> range = 
    m_alienMap.equal_range(key);
  
  for (AlienScopeMap::const_iterator it = range.first; 
       (it != range.second); ++it) {
    // we know that filenm and procnm match
    AlienScope* a = it->second;
    
    if ( (IsValidLine(line) && containsLineFzy(a, line))
	 || (!IsValidLine(a->begLine())) ) {
      alien = a;  // FIXME: potentially more than one...
      break;
    }
  }
  
  if (!alien) {
    Ctxt& top_ctxt = m_ctxtStack.front();
    alien = new AlienScope(top_ctxt.scope(), filenm, procnm, line, line);
    m_alienMap.insert(std::make_pair(key, alien));
  }

  return alien;
}


#if 0
// LocateStmt: Locate the instruction 'insn' within a procedure given
// only basic symbolic information.  Assumes that the
// StatementRangeScope does not live a loop.
static StmtRangeScope*
LocateStmt(binutils::Insn* insn, VMAInterval& vmaint, 
	   LoadModScope* lmScope,
	   string& filenm, string& procnm, suint line)
{
  // 1. Find containing file
  string fnm = (filenm.empty()) ? OrphanedProcedureFile : filenm;
  FileScope* fScope = FindOrCreateFileNode1(lmScope, fnm);
  
  // 2. Find containing procedure 
  // FIXME: we would like to potentially infer multiple procs
  string pnm = (procnm.empty()) ? InferredProcedure : procnm;
  ProcScope* pScope = FindOrCreateProcNode1(fScope, pnm, line);
  pScope->vmaSet().insert(vmaint); // expand VMA range
  
  // 3. Locate StmtRangeScope within the procedure
  StmtRangeScope* stmt = pScope->FindStmtRange(line);
  if (!stmt) {
    stmt = new StmtRangeScope(pScope, line, line, vmaint.beg(), vmaint.end());
  }
  else {
    stmt->vmaSet().insert(vmaint); // expand VMA range
  }
  
  return stmt;
}
#endif


} // namespace bloop

} // namespace banal
