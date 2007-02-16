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

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <fstream>
#include <sstream>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <string>
using std::string;

#include <map>  // STL
#include <list> // STL

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/CFG/ManagerCFGStandard.hpp>
#include <OpenAnalysis/Utils/RIFG.hpp>
#include <OpenAnalysis/Utils/NestedSCR.hpp>
#include <OpenAnalysis/Utils/Exception.hpp>

//*************************** User Include Files ****************************

#include "bloop.hpp"
#include "bloop_LocationMgr.hpp"
#include "OAInterface.hpp"

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations ***************************

namespace banal {

namespace bloop {

// ------------------------------------------------------------
// Helpers for building a scope tree
// ------------------------------------------------------------

typedef std::multimap<ProcScope*, binutils::Proc*> ProcScopeToProcMap;


static ProcScopeToProcMap*
BuildLMSkeleton(LoadModScope *lmScope, binutils::LM* lm);

static FileScope*
FindOrCreateFileNode(LoadModScope* lmScope, binutils::Proc* p);

static ProcScope*
FindOrCreateProcNode(FileScope* fScope, binutils::Proc* p);


static ProcScope*
BuildProcStructure(ProcScope* pScope, binutils::Proc* p, 
		   bool irreducibleIntervalIsLoop); 

static int
BuildProcLoopNests(ProcScope* pScope, binutils::Proc* p,
		   bool irreducibleIntervalIsLoop);

static int
BuildStmts(bloop::LocationMgr& locMgr,
	   CodeInfo* enclosingScope, binutils::Proc* p,
	   OA::OA_ptr<OA::CFG::Interface::Node> bb);


static bool
WasCtxtClosed(CodeInfo* scope, LocationMgr& mgr);

static void
FindLoopBegLineInfo(binutils::Proc* p, 
		    OA::OA_ptr<OA::CFG::Interface::Node> headBB,
		    string& begFilenm, string& begProcnm, suint& begLn);

static StmtRangeScope*
FindOrCreateStmtNode(std::map<suint, StmtRangeScope*>& stmtMap,
		     CodeInfo* enclosingScope, suint line, 
		     VMAInterval& vmaint);


// Cannot make this a local class because it is used as a template
// argument! Sigh.
class QNode {
public:
  QNode(OA::RIFG::NodeId x = OA::RIFG::NIL, 
	CodeInfo* y = NULL, CodeInfo* z = NULL)
    : fgNode(x), enclosingScope(y), scope(z) { }
  
  bool isProcessed() const { return (scope != NULL); }
  
  OA::RIFG::NodeId fgNode;  // flow graph node
  CodeInfo* enclosingScope; // enclosing scope of 'fgNode'
  CodeInfo* scope;          // scope for children of 'fgNode' (fgNode scope)
};

  
} // namespace bloop

} // namespace banal


//*************************** Forward Declarations ***************************

namespace banal {

namespace bloop {

// ------------------------------------------------------------
// Helpers for normalizing the scope tree
// ------------------------------------------------------------

static bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree);

static bool 
MergeBogusAlienScopes(PgmScopeTree* pgmScopeTree);

static bool
CoalesceDuplicateStmts(PgmScopeTree* pgmScopeTree, bool unsafeNormalizations);

static bool 
MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree);

static bool 
RemoveEmptyScopes(PgmScopeTree* pgmScopeTree);

static bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, 
			 const char* canonicalPathList);

// ------------------------------------------------------------
// Helpers for normalizing the scope tree
// ------------------------------------------------------------
class StmtData;

class LineToStmtMap : public std::map<suint, StmtData*> {
public:
  typedef std::map<suint, StmtData*> My_t;

public:
  LineToStmtMap() { }
  virtual ~LineToStmtMap() { clear(); }
  virtual void clear();
};

class StmtData {
public:
  StmtData(StmtRangeScope* _stmt = NULL, int _level = 0)
    : stmt(_stmt), level(_level) { }
  ~StmtData() { /* owns no data */ }

  StmtRangeScope* GetStmt() const { return stmt; }
  int GetLevel() const { return level; }
  
  void SetStmt(StmtRangeScope* _stmt) { stmt = _stmt; }
  void SetLevel(int _level) { level = _level; }
  
private:
  StmtRangeScope* stmt;
  int level;
};

static void 
DeleteContents(ScopeInfoSet* s);


// ------------------------------------------------------------
// Helpers for normalizing the scope tree
// ------------------------------------------------------------

class CDS_RestartException {
public:
  CDS_RestartException(CodeInfo* n) : node(n) { }
  ~CDS_RestartException() { }
  CodeInfo* GetNode() const { return node; }

private:
  CodeInfo* node;
};

} // namespace bloop

} // namespace banal

//*************************** Forward Declarations ***************************

#define DBG_PROC 0 /* debug BuildProcStructure */
#define DBG_CDS  0 /* debug CoalesceDuplicateStmts */

static string OrphanedProcedureFile =
  "~~~<unknown-file>~~~";

static string InferredProcedure =
  "~<unknown-proc>~";

static const char *PGMdtd =
#include <lib/xml/PGM.dtd.h>

//****************************************************************************
// Set of routines to write a scope tree
//****************************************************************************

// FIXME (minor): move to prof-juicy library for experiment writer
void
banal::bloop::WriteScopeTree(std::ostream& os, PgmScopeTree* pgmScopeTree, 
			     bool prettyPrint)
{
  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE PGM [\n" << PGMdtd << "]>" << std::endl;
  os.flush();

  int dumpFlags = (PgmScopeTree::XML_TRUE); // ScopeInfo::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= PgmScopeTree::COMPRESSED_OUTPUT; }
  
  pgmScopeTree->xml_dump(os, dumpFlags);
}

//****************************************************************************
// Set of routines to build a scope tree
//****************************************************************************

// BuildLMStructure: Builds a scope tree -- with a focus on loop nest
//   recovery -- representing program source code from the load module
//   'lm'.
// A load module represents binary code, which is essentially
//   a collection of procedures along with some extra information like
//   symbol and debugging tables.  This routine relies on the debugging
//   information to associate procedures with thier source code files
//   and to recover procedure loop nests, correlating them to source
//   code line numbers.  A normalization pass is typically run in order
//   to 'clean up' the resulting scope tree.  Among other things, the
//   normalizer employs heuristics to reverse certain compiler
//   optimizations such as loop unrolling.
PgmScopeTree*
banal::bloop::BuildLMStructure(binutils::LM* lm, 
			       const char* canonicalPathList, 
			       bool normalizeScopeTree,
			       bool unsafeNormalizations,
			       bool irreducibleIntervalIsLoop)
{
  DIAG_Assert(lm, DIAG_UnexpectedInput);

  PgmScopeTree* pgmScopeTree = NULL;
  PgmScope* pgmScope = NULL;

  // FIXME (minor): relocate
  OrphanedProcedureFile = "~~~" + lm->GetName() + ":<unknown-file>~~~";

  // Assume lm->Read() has been performed
  pgmScope = new PgmScope("");
  pgmScopeTree = new PgmScopeTree("", pgmScope);

  LoadModScope* lmScope = new LoadModScope(lm->GetName(), pgmScope);

  // 1. Build FileScope/ProcScope skeletal structure
  ProcScopeToProcMap* mp = BuildLMSkeleton(lmScope, lm);
  
  // 2. For each [ProcScope, binutils::Proc] pair, complete the build.
  // Note that a ProcScope may be associated with more than one
  // binutils::Proc.
  for (ProcScopeToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    ProcScope* pScope = it->first;
    binutils::Proc* p = it->second;

    DIAG_Msg(2, "Building scope tree for [" << p->GetName()  << "] ... ");
    BuildProcStructure(pScope, p, irreducibleIntervalIsLoop);
  }
  delete mp;

  // 3. Normalize
  if (canonicalPathList && canonicalPathList[0] != '\0') {
    bool result = FilterFilesFromScopeTree(pgmScopeTree, canonicalPathList);
    DIAG_Assert(result, "Path canonicalization result should never be false!");
  }

  if (normalizeScopeTree) {
    bool result = Normalize(pgmScopeTree, unsafeNormalizations);
    DIAG_Assert(result, "Normalization result should never be false!");
  }

  return pgmScopeTree;
}


// Normalize: Because of compiler optimizations and other things, it
// is almost always desirable normalize a scope tree.  For example,
// almost all unnormalized scope tree contain duplicate statement
// instances.  See each normalizing routine for more information.
bool 
banal::bloop::Normalize(PgmScopeTree* pgmScopeTree, bool unsafeNormalizations)
{
  bool changed = false;
  
  // changed |= RemoveOrphanedProcedureRepository(pgmScopeTree);

  // Remove unnecessary alien scopes
  changed |= MergeBogusAlienScopes(pgmScopeTree);

  // Cleanup procedure/alien scopes
  changed |= CoalesceDuplicateStmts(pgmScopeTree, unsafeNormalizations);
  changed |= MergePerfectlyNestedLoops(pgmScopeTree);
  changed |= RemoveEmptyScopes(pgmScopeTree);
  
  return true; // no errors
}


//****************************************************************************
// Helpers for building a scope tree
//****************************************************************************

namespace banal {

namespace bloop {

// BuildLMSkeleton: Build skeletal file-procedure structure.  This
// will be useful later when detecting alien lines.  Also, the
// nesting structure allows inference of accurate boundaries on
// procedure end lines.
//
// A ProcScope can be associated with multiple binutils::Procs
//
// ProcScopes will be sorted by begLn (cf. CodeInfo::Reorder)
static ProcScopeToProcMap*
BuildLMSkeleton(LoadModScope* lmScope, binutils::LM* lm)
{
  ProcScopeToProcMap* mp = new ProcScopeToProcMap;
  
  // -------------------------------------------------------
  // 1. Create basic structure for each procedure
  // -------------------------------------------------------
  for (binutils::LMSegIterator it(*lm); it.IsValid(); ++it) {
    binutils::Seg* seg = it.Current();
    if (seg->GetType() != binutils::Seg::Text) {
      continue;
    }
    
    // We have a text segment.  Iterate over procedures.
    binutils::TextSeg* tseg = dynamic_cast<binutils::TextSeg*>(seg);
    for (binutils::TextSegProcIterator it1(*tseg); it1.IsValid(); ++it1) {
      binutils::Proc* p = it1.Current();
      FileScope* fScope = FindOrCreateFileNode(lmScope, p);
      ProcScope* pScope = FindOrCreateProcNode(fScope, p);
      mp->insert(make_pair(pScope, p));
    }
  }

  // -------------------------------------------------------
  // 2. Establish nesting information:
  // -------------------------------------------------------
  for (ProcScopeToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    ProcScope* pScope = it->first;
    binutils::Proc* p = it->second;
    binutils::Proc* parent = p->parent();

    if (parent) {
      ProcScope* parentScope = lmScope->findProc(parent->GetBegVMA());
      pScope->Unlink();
      pScope->Link(parentScope);
    }
  }

  // FIXME (minor): The following order is appropriate when we have
  // symbolic information. I.e. we, 1) establish nesting information
  // and then attempt to refine procedure bounds using nesting
  // information.  If such nesting information is not available,
  // assume correct bounds and attempt to establish nesting.
  
  // 3. Establish procedure bounds: nesting + first line ... [FIXME]

  // 4. Establish procedure groups: [FIXME: more stuff from DWARF]
  //      template instantiations, class member functions
  return mp;
}


// FindOrCreateFileNode: 
static FileScope* 
FindOrCreateFileNode(LoadModScope* lmScope, binutils::Proc* p)
{
  // Attempt to find filename for procedure
  string filenm = p->GetFilename();
  if (filenm.empty()) {
    string procnm;
    suint line;
    p->GetSourceFileInfo(p->GetBegVMA(), 0, procnm, filenm, line);
  }
  if (filenm.empty()) { 
    filenm = OrphanedProcedureFile; 
  }
  
  // Obtain corresponding FileScope
  return FileScope::findOrCreate(lmScope, filenm);
} 


// FindOrCreateProcNode: Build skeletal ProcScope.  We can assume that
// the parent is always a FileScope.
static ProcScope*
FindOrCreateProcNode(FileScope* fScope, binutils::Proc* p)
{
  // Find VMA boundaries: [beg, end)
  VMA endVMA = p->GetBegVMA() + p->GetSize();
  VMAInterval bounds(p->GetBegVMA(), endVMA);
  DIAG_Assert(!bounds.empty(), "Attempting to add empty procedure " 
	      << bounds.toString());
  
  // Find procedure name
  string procNm   = GetBestFuncName(p->GetName()); 
  string procLnNm = GetBestFuncName(p->GetLinkName());
  
  // Find preliminary source line bounds
  string file, proc;
  suint begLn1, endLn1;
  binutils::Insn* eInsn = p->GetLastInsn();
  ushort endOp = (eInsn) ? eInsn->GetOpIndex() : 0;
  p->GetSourceFileInfo(p->GetBegVMA(), 0, p->GetEndVMA(), endOp, 
		       proc, file, begLn1, endLn1, 0 /*no swapping*/);
  
  // Compute source line bounds to uphold invariant:
  //   (begLn == 0) <==> (endLn == 0)
  suint begLn, endLn;
  if (p->hasSymbolic()) {
    begLn = p->GetBegLine();
    endLn = MAX(begLn, endLn1);
  }
  else {
    // for now, always assume begLn to be more accurate
    begLn = begLn1;
    endLn = MAX(begLn1, endLn1);
  }
  
  // Create or find the scope.  Fuse procedures if names match.
  ProcScope* pScope = fScope->FindProc(procNm, procLnNm);
  if (!pScope) {
    pScope = new ProcScope(procNm, fScope, procLnNm, begLn, endLn);
  }
  else {
    // Assume the procedure was split.  Fuse it together.
    DIAG_DevMsg(3, "Merging multiple instances of procedure [" 
		<< pScope->toStringXML() << "] with " << procNm << " " 
		<< procLnNm << " " << bounds.toString());
    pScope->ExpandLineRange(begLn, endLn);
  }
  pScope->vmaSet().insert(bounds);
  
  return pScope;
}

} // namespace bloop

} // namespace banal


//****************************************************************************
// 
//****************************************************************************

#if (DBG_PROC)
static bool DBG_PROC_print_now = false;
#endif

namespace banal {

namespace bloop {


static int 
BuildProcLoopNests(ProcScope* enclosingProc, binutils::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::Interface> cfg, 
		   OA::RIFG::NodeId fgRoot, 
		   bool irrIntIsLoop);

static CodeInfo*
BuildLoopAndStmts(bloop::LocationMgr& locMgr, 
		  CodeInfo* enclosingScope, binutils::Proc* p,
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::Interface> cfg, 
		  OA::RIFG::NodeId fgNode, bool irrIntIsLoop);


// BuildProcStructure: Complete the representation for 'pScope' given the
// binutils::Proc 'p'.  Note that pScopes parent may itself be a ProcScope.
static ProcScope* 
BuildProcStructure(ProcScope* pScope, binutils::Proc* p,
		   bool irreducibleIntervalIsLoop)
{
  DIAG_Msg(3, "==> Proc `" << p->GetName() << "' (" << p->GetId() << ") <==");
  
#if (DBG_PROC)
  DBG_PROC_print_now = false;
  suint dbgId = p->GetId();

  const char* dbgNm = "xxx";
  if (p->GetName().find(dbgNm) != string::npos) {
    DBG_PROC_print_now = true;
  }
  if (dbgId == 10) {
    DBG_PROC_print_now = true; 
  }
#endif
  
  BuildProcLoopNests(pScope, p, irreducibleIntervalIsLoop);
  
  return pScope;
}


// BuildProcLoopNests: Build procedure structure by traversing
// the Nested SCR (Tarjan tree) to create loop nests and statement
// scopes.
static int
BuildProcLoopNests(ProcScope* pScope, binutils::Proc* p,
		   bool irreducibleIntervalIsLoop)
{
  try {
    using banal::OAInterface;
    
    OA::OA_ptr<OAInterface> irIF; irIF = new OAInterface(p);
    
    OA::OA_ptr<OA::CFG::ManagerStandard> cfgmanstd;
    cfgmanstd = new OA::CFG::ManagerStandard(irIF);
    OA::OA_ptr<OA::CFG::CFGStandard> cfg = 
      cfgmanstd->performAnalysis(TY_TO_IRHNDL(p, OA::ProcHandle));
    
    OA::OA_ptr<OA::RIFG> rifg; 
    rifg = new OA::RIFG(cfg, cfg->getEntry(), cfg->getExit());
    OA::OA_ptr<OA::NestedSCR> tarj; tarj = new OA::NestedSCR(rifg);
    
    OA::RIFG::NodeId fgRoot = rifg->getSource();

#if (DBG_PROC)
    if (DBG_PROC_print_now) {
      cout << "*** CFG for `" << p->GetName() << "' ***" << endl;
      cout << "  total blocks: " << cfg->getNumNodes() << endl
	   << "  total edges:  " << cfg->getNumEdges() << endl;
      cfg->dump(cout, irIF);
      cfg->dumpdot(cout, irIF);
      
      cout << "*** Nested SCR (Tarjan Interval) Tree for `" << 
	p->GetName() << "' ***" << endl;
      tarj->dump(cout);
      cout << endl;
      cout.flush(); cerr.flush();
    }
#endif

    int r = BuildProcLoopNests(pScope, p, tarj, cfg, fgRoot,
			       irreducibleIntervalIsLoop);
    return r;
  }
  catch (const OA::Exception& x) {
    std::ostringstream os;
    x.report(os);
    DIAG_Throw("[OpenAnalysis] " << os.str());
  }
}


// BuildProcLoopNests: 
//
// FIXME (minor, comments)
//
//   Visit the nested interval tree in a hybrid BFS+DFS order and construct the loop nest... [relocate while visiting]  The algorithm is designed to visit loops from one routine in DFS order.  When there is no inlining, this collapses to a pure BFS.  In the presence of inlining, ... ***  Behavior is dependent on 'IsInCurrentCtxt'. ***
//
//  Within a procedure, we visit in BFS order so that we can relocate and check loop boundaries before visiting inner scopes (which 
// 
//   (Tarjan interval) analysis and returns the number of loops created.
// 
// We visit the interval tree in DFS order which is equivalent to
// visiting each basic block in VMA order.
//
// Note that these loops are UNNORMALIZED.
static int 
BuildProcLoopNests(ProcScope* enclosingProc, binutils::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::Interface> cfg, 
		   OA::RIFG::NodeId fgRoot, 
		   bool irrIntIsLoop)
{
  typedef std::list<QNode> MyQueue;

  int nLoops = 0;
  
  bloop::LocationMgr locMgr(enclosingProc->LoadMod());
#if DBG_PROC
  if (DBG_PROC_print_now) {
    locMgr.debug(1);
  }
#endif
  locMgr.begSeq(enclosingProc);
  
  // -------------------------------------------------------
  // BFS+DFS on the Nested SCR (Tarjan tree)
  // -------------------------------------------------------
  // Queue INVARIANTs.  The queue has two sections to support the
  // hybrid BFS+DFS search:
  //
  //             BFS sec.      TODO sec.
  //   queue: [ e_i ... e_j | e_k ... e_l ]
  //            ^             ^
  //            begin()       q_todo
  // 
  //  1.  Nodes in BFS section have already been processed.
  //  1a. Processed nodes have non-NULL child-scopes
  //  2.  Nodes in TODO section have *not* been processed.
  //  2a. Non-processed nodes have NULL child-scopes
  //  3.  No node's children has been processed
  MyQueue theQueue;
  theQueue.push_back(QNode(fgRoot, enclosingProc, NULL));
  MyQueue::iterator q_todo = theQueue.begin();

  while (!theQueue.empty()) {
    // -------------------------------------------------------
    // 1. pop the front element, adjusting q_todo if necessary
    // -------------------------------------------------------
    bool isTODO = (q_todo == theQueue.begin());
    if (isTODO) {
      q_todo++;
    }
    QNode qnode = theQueue.front();
    theQueue.pop_front();

    // -------------------------------------------------------
    // 2. process the element, if necessary
    // -------------------------------------------------------
    if (isTODO) {
      // Note that if this call closes an alien context, invariants
      // below ensure that this context has been fully explored.
      CodeInfo* myScope;
      myScope = BuildLoopAndStmts(locMgr, qnode.enclosingScope, p,
				  tarj, cfg, qnode.fgNode, irrIntIsLoop);
      qnode.scope = myScope;
      if (myScope->Type() == ScopeInfo::LOOP) {
	nLoops++;
      }
    }
    
    // -------------------------------------------------------
    // 3. process children within this context in BFS fashion
    //    (Note: WasCtxtClosed() always true  -> DFS
    //           WasCtxtClosed() always false -> BFS
    // -------------------------------------------------------
    OA::RIFG::NodeId kid = tarj->getInners(qnode.fgNode);
    if (kid == OA::RIFG::NIL) {
      continue;
    }
    
    do {
      CodeInfo* myScope;
      myScope = BuildLoopAndStmts(locMgr, qnode.scope, p, 
				  tarj, cfg, kid, irrIntIsLoop);
      if (myScope->Type() == ScopeInfo::LOOP) {
	nLoops++;
      }
      
      // Insert to BFS section (inserts before)
      theQueue.insert(q_todo, QNode(kid, qnode.scope, myScope));
      kid = tarj->getNext(kid);
    }
    while (kid != OA::RIFG::NIL && !WasCtxtClosed(qnode.scope, locMgr));
    
    // FIXME: adjust bounds using loop nesting invariants.

    // -------------------------------------------------------
    // 4. place rest of children in queue's TODO section
    // -------------------------------------------------------
    for ( ; kid != OA::RIFG::NIL; kid = tarj->getNext(kid)) {
      theQueue.push_back(QNode(kid, qnode.scope, NULL));

      // ensure 'q_todo' to points to the beginning of TODO section
      if (q_todo == theQueue.end()) {
	q_todo--;
      }
    }
  }
  
  locMgr.endSeq();

  return nLoops;
}


// BuildLoopAndStmts: Returns the expected (or 'original') enclosing
// scope for children of 'fgNode', e.g. loop or proc. 
static CodeInfo*
BuildLoopAndStmts(bloop::LocationMgr& locMgr, 
		  CodeInfo* enclosingScope, binutils::Proc* p,
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::Interface> cfg, 
		  OA::RIFG::NodeId fgNode, bool irrIntIsLoop)
{
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::Interface::Node> bb = 
    rifg->getNode(fgNode).convert<OA::CFG::Interface::Node>();
  binutils::Insn* insn = banal::OA_CFG_getBegInsn(bb);
  VMA begVMA = (insn) ? insn->GetOpVMA() : 0;
  
  DIAG_DevMsg(50, "BuildLoopAndStmts: " << bb << hex << begVMA << " --> " << enclosingScope << dec << " " << enclosingScope->toString_id());

  CodeInfo* childScope = enclosingScope;

  OA::NestedSCR::Node_t ity = tarj->getNodeType(fgNode);
  if (ity == OA::NestedSCR::NODE_ACYCLIC) {
    // -----------------------------------------------------
    // ACYCLIC: No loops
    // -----------------------------------------------------
  }
  else if (ity == OA::NestedSCR::NODE_INTERVAL || 
	   (irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) {
    // -----------------------------------------------------
    // INTERVAL or IRREDUCIBLE as a loop: Loop head
    // -----------------------------------------------------
    string fnm, pnm;
    suint line;
    FindLoopBegLineInfo(p, bb, fnm, pnm, line);
    pnm = GetBestFuncName(pnm);
    
    LoopScope* loop = new LoopScope(NULL, line, line);
    loop->vmaSet().insert(begVMA, begVMA + 1); // a loop id
    locMgr.locate(loop, enclosingScope, fnm, pnm, line);
    childScope = loop;
  }
  else if (!irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
    // -----------------------------------------------------
    // IRREDUCIBLE as no loop: May contain loops
    // -----------------------------------------------------
  }
  else {
    DIAG_Die("Should never reach!");
  }

  // -----------------------------------------------------
  // Process instructions within BB
  // -----------------------------------------------------
  BuildStmts(locMgr, childScope, p, bb);

  return childScope;
}


// BuildStmts: Form loop structure by setting bounds and adding
// statements from the current basic block to 'enclosingScope'.  Does
// not add duplicates.
static int 
BuildStmts(bloop::LocationMgr& locMgr,
	   CodeInfo* enclosingScope, binutils::Proc* p,
	   OA::OA_ptr<OA::CFG::Interface::Node> bb)
{
  OA::OA_ptr<OA::CFG::Interface::NodeStatementsIterator> it =
    bb->getNodeStatementsIterator();
  for ( ; it->isValid(); ) {
    binutils::Insn* insn = IRHNDL_TO_TY(it->current(), binutils::Insn*);
    VMA vma = insn->GetVMA();
    ushort opIdx = insn->GetOpIndex();
    VMA opvma = insn->GetOpVMA();
    
    // advance iterator [needed when creating VMA interval]
    ++(*it);

    // 1. gather source code info
    string filenm, procnm;
    suint line;
    p->GetSourceFileInfo(vma, opIdx, procnm, filenm, line); 
    procnm = GetBestFuncName(procnm);

    // 2. create a VMA interval
    // the next (or hypothetically next) insn begins no earlier than:
    binutils::Insn* n_insn = (it->isValid()) ? 
      IRHNDL_TO_TY(it->current(), binutils::Insn*) : NULL;
    VMA n_opvma = (n_insn) ? n_insn->GetOpVMA() : insn->GetEndVMA();
    DIAG_Assert(opvma < n_opvma, "Invalid VMAInterval: [" << opvma << ", "
		<< n_opvma << ")");

    VMAInterval vmaint(opvma, n_opvma);

    // 3. locate stmt
    StmtRangeScope* stmt = 
      new StmtRangeScope(NULL, line, line, vmaint.beg(), vmaint.end());
    locMgr.locate(stmt, enclosingScope, filenm, procnm, line);
  }
  return 0;
} 


#if 0 // FIXME: Deprecated
// BuildProcLoopNests: Recursively build loops using Nested SCR
// (Tarjan interval) analysis and returns the number of loops created.
// 
// We visit the interval tree in DFS order which is equivalent to
// visiting each basic block in VMA order.
//
// Note that these loops are UNNORMALIZED.
static int 
BuildProcLoopNests(CodeInfo* enclosingScope, binutils::Proc* p, 
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::Interface> cfg, 
		   OA::RIFG::NodeId fgNode, 
		   int addStmts, bool irrIntIsLoop)
{
  int localLoops = 0;
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::Interface::Node> bb =
    rifg->getNode(fgNode).convert<OA::CFG::Interface::Node>();
  
  DIAG_DevMsg(50, "BuildProcLoopNests: " << bb <<  " --> "  << hex << enclosingScope << dec << " " << enclosingScope->toString_id());

  if (addStmts) {
    // mp->push_back(make_pair(bb, enclosingScope));
  }
  
  // -------------------------------------------------------
  // Recursively traverse the Nested SCR (Tarjan tree), building loop nests
  // -------------------------------------------------------
  for (int kid = tarj->getInners(fgNode); kid != OA::RIFG::NIL; 
       kid = tarj->getNext(kid) ) {
    OA::OA_ptr<OA::CFG::Interface::Node> bb1 = 
      rifg->getNode(kid).convert<OA::CFG::Interface::Node>();
    OA::NestedSCR::Node_t ity = tarj->getNodeType(kid);
    
    if (ity == OA::NestedSCR::NODE_ACYCLIC) { 
      // -----------------------------------------------------
      // ACYCLIC: No loops
      // -----------------------------------------------------
      if (addStmts) {
	//mp->push_back(make_pair(bb1, enclosingScope));
      }
    }
    else if (ity == OA::NestedSCR::NODE_INTERVAL || 
	     (irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) {
      // -----------------------------------------------------
      // INTERVAL or IRREDUCIBLE as a loop: Loop head
      // -----------------------------------------------------

      // add alien context if necessary....
      LoopScope* lScope = new LoopScope(enclosingScope, line, line);
      int num = BuildProcLoopNests(lScope, p, tarj, cfg, kid, mp,
				   1, irrIntIsLoop);
      localLoops += (num + 1);
    }
    else if (!irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
      // -----------------------------------------------------
      // IRREDUCIBLE as no loop: May contain loops
      // -----------------------------------------------------
      int num = BuildProcLoopNests(enclosingScope, p, tarj, cfg, kid, mp,
				   addStmts, irrIntIsLoop);
      localLoops += num;
    }
    else {
      DIAG_Die("Should never reach!");
    }
  }
  
  return localLoops;
}
#endif


//****************************************************************************
// 
//****************************************************************************

// WasCtxtClosed: Given a context 'scope' and a LocationMgr, returns true if
// LocationMgr has closed 'scope'.
static bool
WasCtxtClosed(CodeInfo* scope, LocationMgr& mgr)
{
#if 0
  return true; // FIXME: old DFS behavior
#else
  // Only alien context can be closed.  See...
  if (scope->Type() == ScopeInfo::ALIEN) {
    // context was closed if it is not on the scope stack
    return (!mgr.isParentScope(scope));
  }
  else {
    return false;
  }
#endif
}


// FindLoopBegLineInfo: Given the head basic block node of the loop,
// find loop begin source line information.  
//
// The routine first attempts to use the source line information for
// the backward branch, if one exists.
//
// Note: It is possible to have multiple or no backward branches
// (e.g. an 'unstructured' loop written with IFs and GOTOs).  In the
// former case, we take the smallest source line of them all; in the
// latter we use headVMA.
static void
FindLoopBegLineInfo(binutils::Proc* p, 
		    OA::OA_ptr<OA::CFG::Interface::Node> headBB,
		    string& begFilenm, string& begProcnm, suint& begLn)
{
  using namespace OA::CFG;

  begLn = UNDEF_LINE;

  // Find the head vma
  binutils::Insn* head = banal::OA_CFG_getBegInsn(headBB);
  VMA headVMA = head->GetVMA();
  ushort headOpIdx = head->GetOpIndex();
  DIAG_Assert(headOpIdx == 0, "Target of a branch at " << headVMA 
	      << " has op-index of: " << headOpIdx);
  
  // Now find the backward branch
  OA::OA_ptr<Interface::IncomingEdgesIterator> it 
    = headBB->getIncomingEdgesIterator();
  for ( ; it->isValid(); ++(*it)) {
    OA::OA_ptr<Interface::Edge> e = it->current();
    
    OA::OA_ptr<Interface::Node> bb = e->source();

    binutils::Insn* backBR = banal::OA_CFG_getEndInsn(bb);
    if (!backBR) {
      continue;
    }
    
    VMA vma = backBR->GetVMA();
    ushort opIdx = backBR->GetOpIndex();

    // If we have a backward edge, find the source line of the
    // backward branch.  Note: back edges are not always labeled as such!
    if (e->getType() == Interface::BACK_EDGE || vma == headVMA) {
      suint line;
      string filenm, procnm;
      p->GetSourceFileInfo(vma, opIdx, procnm, filenm, line); 
      if (IsValidLine(line) && (!IsValidLine(begLn) || line < begLn)) {
	begLn = line;
	begFilenm = filenm;
	begProcnm = procnm;
      }
    }
  }
  
  if (!IsValidLine(begLn)) {
    VMA headOpIdx = head->GetOpIndex();
    p->GetSourceFileInfo(headVMA, headOpIdx, begProcnm, begFilenm, begLn);
  }
}



// FindOrCreateStmtNode: Build a StmtRangeScope.  Unlike LocateStmt,
// assumes that procedure boundaries do not need to be expanded.
static StmtRangeScope*
FindOrCreateStmtNode(std::map<suint, StmtRangeScope*>& stmtMap,
		     CodeInfo* enclosingScope, suint line, VMAInterval& vmaint)
{
  StmtRangeScope* stmt = stmtMap[line];
  if (!stmt) {
    stmt = new StmtRangeScope(enclosingScope, line, line, 
			      vmaint.beg(), vmaint.end());
    stmtMap.insert(make_pair(line, stmt));
  }
  else {
    stmt->vmaSet().insert(vmaint); // expand VMA range
  }
  return stmt;
}

} // namespace bloop

} // namespace banal


//****************************************************************************
// Helpers for normalizing a scope tree
//****************************************************************************

namespace banal {

namespace bloop {

// RemoveOrphanedProcedureRepository: Remove the OrphanedProcedureFile
// from the tree.
static bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree)
{
  bool changed = false;
  
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return changed; }
  
  for (ScopeInfoIterator it(pgmScope, &ScopeTypeFilter[ScopeInfo::FILE]); 
       it.Current(); /* */) {
    FileScope* file = dynamic_cast<FileScope*>(it.Current());
    it++; // advance iterator -- it is pointing at 'file'
    
    if (file->name() == OrphanedProcedureFile) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
      changed = true;
    }
  } 
  
  return changed;
}


//****************************************************************************

static bool 
MergeBogusAlienScopes(CodeInfo* node, FileScope* file);


static bool 
MergeBogusAlienScopes(PgmScopeTree* pgmScopeTree)
{
  bool changed = false;
  
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return changed; }
  
  for (ScopeInfoIterator it(pgmScope, &ScopeTypeFilter[ScopeInfo::PROC]);
       it.Current(); ++it) {
    ProcScope* proc = dynamic_cast<ProcScope*>(it.Current());
    FileScope* file = proc->File();
    changed |= MergeBogusAlienScopes(proc, file);
  }
  
  return changed;
}


static bool 
MergeBogusAlienScopes(CodeInfo* node, FileScope* file)
{
  bool changed = false;
  
  if (!node) { return changed; }

  for (CodeInfoChildIterator it(node); it.Current(); /* */) {
    CodeInfo* child = it.CurCodeInfo();
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    changed |= MergeBogusAlienScopes(child, file);
    
    // 2. Merge an alien node if it is redundant with its calling context
    if (child->Type() == ScopeInfo::ALIEN) {
      AlienScope* alien = dynamic_cast<AlienScope*>(child);
      CodeInfo* parent = alien->CodeInfoParent();
      
      CodeInfo* callCtxt = parent->CallingCtxt();
      const string& callCtxtFnm = (callCtxt->Type() == ScopeInfo::ALIEN) ?
	dynamic_cast<AlienScope*>(callCtxt)->fileName() : file->name();
      
      if (alien->fileName() == callCtxtFnm
	  && ctxtNameEqFuzzy(callCtxt, alien->name())
	  && LocationMgr::containsIntervalFzy(parent, alien->begLine(), 
					      alien->endLine()))  {
	// Move all children of 'alien' into 'parent'
	changed = ScopeInfo::Merge(parent, alien);
	DIAG_Assert(changed, "MergeBogusAlienScopes: merge failed.");
      }
    }
  }
  
  return changed;
}


//****************************************************************************

// CoalesceDuplicateStmts: Coalesce duplicate statement instances that
// may appear in the scope tree.  There are two basic cases:
//
// Case 1a:
// If the same statement exists at multiple levels within a loop nest,
//   discard all but the innermost instance.
// Rationale: Compiler optimizations may hoist loop-invariant
//   operations out of inner loops.  Note however that in rare cases,
//   code may be moved deeper into a nest (e.g. to free registers).
//   Even so, we want to associate statements within loop nests to the
//   deepest level in which they appear.
// E.g.: lca --- ... --- s1
//          \--- s2
//
// Case 1b:
// If the same statement exists within the same loop, discard all but one.
// Rationale: Multiple statements exist at the same level because of
//   multiple basic blocks containing the same statement, cf. BuildStmts(). [FIXME -- LocationMgr]
//   Also, the merging in case 2 may result in duplicate statements.
// E.g.: lca --- s1
//          \--- s2
//
// Case 2: 
// If duplicate statements appear in different loops, find the least
//   common ancestor (deepest nested common ancestor) in the scope tree
//   and merge the corresponding loops along the paths to each of the
//   duplicate statements.  Note that merging can create instances of
//   case 1.
// Rationale: Compiler optimizations such as loop unrolling (start-up,
//   steady-state, wind-down), e.g., can produce multiple statement
//   instances.
// E.g.: lca ---...--- s1
//          \---...--- s2
static bool CDS_unsafeNormalizations = true;

static bool
CoalesceDuplicateStmts(CodeInfo* scope, LineToStmtMap* stmtMap, 
		       ScopeInfoSet* visited, ScopeInfoSet* toDelete,
		       int level);

static bool
CDS_Main(CodeInfo* scope, LineToStmtMap* stmtMap, 
	 ScopeInfoSet* visited, ScopeInfoSet* toDelete, int level);

static bool
CDS_InspectStmt(StmtRangeScope* stmt1, LineToStmtMap* stmtMap, 
		ScopeInfoSet* toDelete, int level);

static bool 
CDS_ScopeFilter(const ScopeInfo& x, long type)
{
  return (x.Type() == ScopeInfo::PROC || x.Type() == ScopeInfo::ALIEN);
}

static bool
CoalesceDuplicateStmts(PgmScopeTree* pgmScopeTree, bool unsafeNormalizations)
{
  bool changed = false;
  CDS_unsafeNormalizations = unsafeNormalizations;
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  LineToStmtMap stmtMap;      // line to statement data map
  ScopeInfoSet visitedScopes; // all children of a scope have been visited
  ScopeInfoSet toDelete;      // nodes to delete

  // Apply the normalization routine to each ProcScope and AlienScope
  // so that 1) we are guaranteed to only process CodeInfos and 2) we
  // can assume that all line numbers encountered are within the same
  // file (keeping the LineToStmtMap simple and fast).  (Children
  // AlienScope's are skipped.)

  ScopeInfoFilter filter(CDS_ScopeFilter, "CDS_ScopeFilter", 0);
  for (ScopeInfoIterator it(pgmScope, &filter); it.Current(); ++it) {
    CodeInfo* scope = dynamic_cast<CodeInfo*>(it.Current());
    changed |= CoalesceDuplicateStmts(scope, &stmtMap, &visitedScopes,
				      &toDelete, 1);
    stmtMap.clear();           // Clear statement table
    visitedScopes.clear();     // Clear visited set
    DeleteContents(&toDelete); // Clear 'toDelete'
  }

  return changed;
}


// CoalesceDuplicateStmts Helper: 
//
// Because instances of case 2 can create instances of case 1, we need
// to restart the algorithm at the lca after merging has been applied.
//
// This has some implications:
// - We can delete nodes during the merging. (Deletion of nodes will
//   invalidate the current *and* ancestor iterators out to the
//   lca!)
// - We cannot simply delete nodes in case 1.  The restarting means
//   that nodes that we have already seen are in the statement map and
//   could therefore be deleted out from under us during iteration. 
//   Consider this snippet of code:
// 
//   <L b=... >  // level 3
//     ...
//     <L b="1484" e="1485">  // level 4
//        <S b="1484" />
//        <S b="1484" />
//        <S b="1485" />
//        <S b="1485" />
//     </L>
//     <S b="1485">
//     ...
//   </L>
//   
//   CDS_Main loops on the children of the current scope.  It saves the
//   current value of the iterator to 'child' and then increments it (an
//   attempt to prevet this problem!). After that it recursively calls CDS
//   on 'child'.
//   
//   When 'child' points to the 4th level loop in the picture above, the
//   iterator has been incremented and points to the next child which
//   happens to be a statement for line 1485.
//   
//   When CDS processes the inner loop and reaches a statement for line
//   1485, it realizes a duplicate was found in the level 3 loop -- the
//   statement to which level 3 loop iterator points to.  Because the
//   current statement is on a deeper level (4 as opposed to 3 in the map),
//   the statement on level 3 is unlinked and deleted.  After the level 4
//   loop is completely processed, it returns back to the level 3 loop
//   where it points to a block of memory that was freed already.
//
// Solution: After application of case 2, unwind the recursion stack
//   and restart the algorithm at the least common ancestor.  Retain a
//   set of visited nodes so that we do not have to revisit fully
//   explored and unchanged nodes.  Since the most recently merged
//   path will not be in the visited set, it will be propertly visited
//
// Notes: 
// - We always merge *into* the current path to avoid deleting all nodes
//   on the current recursion stack.
// - Note that the merging of CodeInfos can trigger a reordering based
//   on begin/end lines; new children will not simply end up at the
//   end of the child list.
//
// Another solution: Divide into two distinct phases.  Phase 1 collects all
//   statements into a multi-map (that handles sorted iteration and
//   fast local resorts).  Phase 2 iterates over the map, applying
//   case 1 and 2 until all duplicate entries are removed.
static bool
CoalesceDuplicateStmts(CodeInfo* scope, LineToStmtMap* stmtMap, 
		       ScopeInfoSet* visited, ScopeInfoSet* toDelete, 
		       int level)
{
  try {
    return CDS_Main(scope, stmtMap, visited, toDelete, level);
  } 
  catch (CDS_RestartException& x) {
    // Unwind the recursion stack until we find the node
    if (x.GetNode() == scope) {
      return CoalesceDuplicateStmts(x.GetNode(), stmtMap, visited, 
				    toDelete, level);
    } 
    else {
      throw;
    }
  }
}


// CDS_Main: Helper for the above. Assumes that all statement line
// numbers are within the same file.  We operate on the children of
// 'scope' to support support node deletion (case 1 above).  When we
// have visited all children of 'scope' we place it in 'visited'.
static bool
CDS_Main(CodeInfo* scope, LineToStmtMap* stmtMap, ScopeInfoSet* visited, 
	 ScopeInfoSet* toDelete, int level)
{
  bool changed = false;
  
  if (!scope) { return changed; }
  if (visited->find(scope) != visited->end()) { return changed; }
  if (toDelete->find(scope) != toDelete->end()) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (ScopeInfoChildIterator it(scope); it.Current(); ++it) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    DIAG_Assert(child, "");
    
    if (toDelete->find(child) != toDelete->end()) { continue; }
    
    if (child->Type() == ScopeInfo::ALIEN) { continue; }
    
    DIAG_DevMsgIf(DBG_CDS, "CDS: " << child);

    // 1. Recursively perform re-nesting on 'child'.
    changed |= CoalesceDuplicateStmts(child, stmtMap, visited, toDelete,
				      level + 1);
    
    // 2. Examine 'child'
    if (child->Type() == ScopeInfo::STMT_RANGE) {
      // Note: 'child' may be deleted or a restart exception may be thrown
      StmtRangeScope* stmt = dynamic_cast<StmtRangeScope*>(child);
      changed |= CDS_InspectStmt(stmt, stmtMap, toDelete, level);
    } 
  }
  
  visited->insert(scope);
  return changed; 
}

  
// CDS_InspectStmt: applies case 1 or 2, as described above
static bool
CDS_InspectStmt(StmtRangeScope* stmt1, LineToStmtMap* stmtMap, 
		ScopeInfoSet* toDelete, int level)
{
  bool changed = false;
  
  suint line = stmt1->begLine();
  StmtData* stmtdata = (*stmtMap)[line];
  if (stmtdata) {
    
    StmtRangeScope* stmt2 = stmtdata->GetStmt();
    DIAG_DevMsgIf(DBG_CDS, " Find: " << stmt1 << " " << stmt2);
    
    // Ensure we have two different instances of the same line
    if (stmt1 == stmt2) { return false; }
    
    // Find the least common ancestor.  At most it should be a
    // procedure scope.
    ScopeInfo* lca = ScopeInfo::LeastCommonAncestor(stmt1, stmt2);
    DIAG_Assert(lca, "");
    
    // Because we have the lca and know that the descendent nodes are
    // statements (leafs), the test for case 1 is very simple:
    bool case1 = (stmt1->Parent() == lca || stmt2->Parent() == lca);
    if (case1) {
      // Case 1: Duplicate statements. Delete shallower one.
      StmtRangeScope* toRemove = NULL;
      
      if (stmtdata->GetLevel() < level) { // stmt2.level < stmt1.level
	toRemove = stmt2;
	stmtdata->SetStmt(stmt1);  // replace stmt2 with stmt1
	stmtdata->SetLevel(level);
      } 
      else { 
	toRemove = stmt1;
      }
      
      toDelete->insert(toRemove);
      stmtdata->GetStmt()->vmaSet().merge(toRemove->vmaSet()); // merge VMAs
      DIAG_DevMsgIf(DBG_CDS, "  Delete: " << toRemove);
      changed = true;
    } 
    else if (CDS_unsafeNormalizations) {
      // Case 2: Duplicate statements in different loops (or scopes).
      // Merge the nodes from stmt2->lca into those from stmt1->lca.
      DIAG_DevMsgIf(DBG_CDS, "  Merge: " << stmt1 << " <- " << stmt2);
      changed = ScopeInfo::MergePaths(lca, stmt1, stmt2);
      if (changed) {
	// We may have created instances of case 1.  Furthermore,
	// while neither statement is deleted ('stmtMap' is still
	// valid), iterators between stmt1 and 'lca' are invalidated.
	// Restart at lca.
	CodeInfo* lca_CI = dynamic_cast<CodeInfo*>(lca);
	DIAG_Assert(lca_CI, "");
	throw CDS_RestartException(lca_CI);
      }
    }
  } 
  else {
    // Add the statement instance to the map
    stmtdata = new StmtData(stmt1, level);
    (*stmtMap)[line] = stmtdata;
    DIAG_DevMsgIf(DBG_CDS, " Map: " << stmt1);
  }
  
  return changed;
}


//****************************************************************************

// MergePerfectlyNestedLoops: Fuse together a child with a parent when
// the child is perfectly nested in the parent.
static bool 
MergePerfectlyNestedLoops(ScopeInfo* node);


static bool 
MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree)
{
  return MergePerfectlyNestedLoops(pgmScopeTree->GetRoot());
}


static bool 
MergePerfectlyNestedLoops(ScopeInfo* node)
{
  bool changed = false;
  
  if (!node) { return changed; }
  
  // A post-order traversal of this node (visit children before parent)...
  for (ScopeInfoChildIterator it(node); it.Current(); /* */) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    DIAG_Assert(child, "");
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    changed |= MergePerfectlyNestedLoops(child);
    
    // 2. Test if this is a perfectly nested loop with identical loop
    //    bounds and merge if so.

    // The following cast may be illegal but the 'perfect nesting' test
    //   will ensure it doesn't cause a problem (Loops are CodeInfo's).
    // Perfectly nested test: child is a loop; parent is a loop; and
    //   this is only child.  
    CodeInfo* n_CI = dynamic_cast<CodeInfo*>(node); 
    bool perfNested = (child->Type() == ScopeInfo::LOOP &&
		       node->Type() == ScopeInfo::LOOP &&
		       node->ChildCount() == 1);
    if (perfNested && IsValidLine(child->begLine(), child->endLine()) &&
	child->begLine() == n_CI->begLine() &&
	child->endLine() == n_CI->endLine()) { 

      // Move all children of 'child' so that they are children of 'node'
      changed = ScopeInfo::Merge(node, child);
      DIAG_Assert(changed, "MergePerfectlyNestedLoops: merge failed.");
    }
  } 
  
  return changed; 
}


//****************************************************************************

// RemoveEmptyScopes: Removes certain 'empty' scopes from the tree,
// always maintaining the top level PgmScope (PGM) scope.  See the
// predicate 'RemoveEmptyScopes_isEmpty' for details.
static bool 
RemoveEmptyScopes(ScopeInfo* node);

static bool 
RemoveEmptyScopes_isEmpty(const ScopeInfo* node);


static bool 
RemoveEmptyScopes(PgmScopeTree* pgmScopeTree)
{
  // Always maintain the top level PGM scope, even if empty
  return RemoveEmptyScopes(pgmScopeTree->GetRoot());
}


static bool 
RemoveEmptyScopes(ScopeInfo* node)
{
  bool changed = false;
  
  if (!node) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (ScopeInfoChildIterator it(node); it.Current(); /* */) {
    ScopeInfo* child = dynamic_cast<ScopeInfo*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any trimming for this tree's children
    changed |= RemoveEmptyScopes(child);

    // 2. Trim this node if necessary
    if (RemoveEmptyScopes_isEmpty(child)) {
      child->Unlink(); // unlink 'child' from tree
      delete child;
      changed = true;
    }
  } 
  
  return changed; 
}


// RemoveEmptyScopes_isEmpty: determines whether a scope is 'empty':
//   true, if a FileScope has no children.
//   true, if a LoopScope or ProcScope has no children *and* an empty
//     VMAIntervalSet.
//   false, otherwise
static bool 
RemoveEmptyScopes_isEmpty(const ScopeInfo* node)
{
  if ((node->Type() == ScopeInfo::FILE || node->Type() == ScopeInfo::ALIEN)
      && node->ChildCount() == 0) {
    return true;
  }
  if ((node->Type() == ScopeInfo::PROC || node->Type() == ScopeInfo::LOOP)
      && node->ChildCount() == 0) {
    const CodeInfo* n = dynamic_cast<const CodeInfo*>(node);
    return n->vmaSet().empty();
  }
  return false;
}


//****************************************************************************

// FilterFilesFromScopeTree: 
static bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, 
			 const char* canonicalPathList)
{
  bool changed = false;
  
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return changed; }
  
  for (ScopeInfoIterator it(pgmScope, &ScopeTypeFilter[ScopeInfo::FILE]); 
       it.Current(); /* */) {
    FileScope* file = dynamic_cast<FileScope*>(it.Current());
    it++; // advance iterator -- it is pointing at 'file'
    
    // Verify this file in the current list of acceptible paths
    string baseFileName = BaseFileName(file->name());
    DIAG_Assert(!baseFileName.empty(), "Invalid path!");
    if (!pathfind(canonicalPathList, baseFileName.c_str(), "r")) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
      changed = true;
    }
  } 
  
  return changed;
}


//****************************************************************************
// Helpers for traversing the Nested SCR (Tarjan Tree)
//****************************************************************************

void
LineToStmtMap::clear()
{
  for (iterator it = begin(); it != end(); ++it) {
    delete (*it).second;
  }
  My_t::clear();
}

//****************************************************************************
// Helper Routines
//****************************************************************************

static void 
DeleteContents(ScopeInfoSet* s)
{
  // Delete nodes in toDelete
  for (ScopeInfoSet::iterator it = s->begin(); it != s->end(); ++it) {
    ScopeInfo* n = (*it);
    n->Unlink(); // unlink 'n' from tree
    delete n;
  }
  s->clear();
}


} // namespace bloop

} // namespace banal

