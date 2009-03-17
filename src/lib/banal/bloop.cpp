// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#include <iomanip>

#include <fstream>
#include <sstream>

#include <string>
using std::string;

#include <map>
#include <list>
#include <vector>

#include <algorithm>

#include <cstring>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/CFG/ManagerCFG.hpp>
#include <OpenAnalysis/Utils/RIFG.hpp>
#include <OpenAnalysis/Utils/NestedSCR.hpp>
#include <OpenAnalysis/Utils/Exception.hpp>

//*************************** User Include Files ****************************

#include "bloop.hpp"
#include "bloop_LocationMgr.hpp"
#include "OAInterface.hpp"

#include <lib/prof-juicy/Struct-Tree.hpp>
using namespace Prof;

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/xml/xml.hpp> 

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/pathfind.h>

//*************************** Forward Declarations ***************************

namespace banal {

namespace bloop {

// ------------------------------------------------------------
// Helpers for building a structure tree
// ------------------------------------------------------------

typedef std::multimap<Struct::Proc*, BinUtil::Proc*> ProcStrctToProcMap;

static ProcStrctToProcMap*
buildLMSkeleton(Struct::LM* lmStrct, BinUtil::LM* lm, ProcNameMgr* procNmMgr);

static Struct::File*
demandFileNode(Struct::LM* lmStrct, BinUtil::Proc* p);

static Struct::Proc*
demandProcNode(Struct::File* fStrct, BinUtil::Proc* p, ProcNameMgr* procNmMgr);


static Struct::Proc*
buildProcStructure(Struct::Proc* pStrct, BinUtil::Proc* p, 
		   bool isIrrIvalLoop, bool isFwdSubst, 
		   ProcNameMgr* procNmMgr, const std::string& dbgProcGlob); 

static int
buildProcLoopNests(Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst, 
		   ProcNameMgr* procNmMgr, bool isDbg);

static int
buildStmts(bloop::LocationMgr& locMgr,
	   Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
	   OA::OA_ptr<OA::CFG::NodeInterface> bb, ProcNameMgr* procNmMgr);


static void
findLoopBegLineInfo(/*Struct::ACodeNode* procCtxt,*/ BinUtil::Proc* p,
		    OA::OA_ptr<OA::CFG::NodeInterface> headBB,
		    string& begFilenm, string& begProcnm, SrcFile::ln& begLn);

#if 0
static Struct::Stmt*
demandStmtNode(std::map<SrcFile::ln, Struct::Stmt*>& stmtMap,
	       Struct::ACodeNode* enclosingStrct, SrcFile::ln line, 
	       VMAInterval& vmaint);
#endif

// Cannot make this a local class because it is used as a template
// argument! Sigh.
class QNode {
public:
  QNode(OA::RIFG::NodeId x = OA::RIFG::NIL, 
	Struct::ACodeNode* y = NULL, Struct::ACodeNode* z = NULL)
    : fgNode(x), enclosingStrct(y), scope(z) { }
  
  bool isProcessed() const { return (scope != NULL); }
  
  OA::RIFG::NodeId fgNode;  // flow graph node
  Struct::ACodeNode* enclosingStrct; // enclosing scope of 'fgNode'
  Struct::ACodeNode* scope;          // scope for children of 'fgNode' (fgNode scope)
};

  
} // namespace bloop

} // namespace banal


//*************************** Forward Declarations ***************************

namespace banal {

namespace bloop {

// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------

static bool 
mergeBogusAlienStrct(Prof::Struct::LM* lmStrct);

static bool
coalesceDuplicateStmts(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe);

static bool 
mergePerfectlyNestedLoops(Struct::ANode* node);

static bool 
removeEmptyNodes(Struct::ANode* node);

  
// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------
class StmtData;

class SortIdToStmtMap : public std::map<int, StmtData*> {
public:
  typedef std::map<int, StmtData*> My_t;

public:
  SortIdToStmtMap() { }
  virtual ~SortIdToStmtMap() { clear(); }
  virtual void clear();
};

class StmtData {
public:
  StmtData(Struct::Stmt* _stmt = NULL, int _level = 0)
    : stmt(_stmt), level(_level) { }
  ~StmtData() { /* owns no data */ }

  Struct::Stmt* GetStmt() const { return stmt; }
  int GetLevel() const { return level; }
  
  void SetStmt(Struct::Stmt* _stmt) { stmt = _stmt; }
  void SetLevel(int _level) { level = _level; }
  
private:
  Struct::Stmt* stmt;
  int level;
};

static void 
deleteContents(Struct::ANodeSet* s);


// ------------------------------------------------------------
// Helpers for normalizing the structure tree
// ------------------------------------------------------------

class CDS_RestartException {
public:
  CDS_RestartException(Struct::ACodeNode* n) : node(n) { }
  ~CDS_RestartException() { }
  Struct::ACodeNode* GetNode() const { return node; }

private:
  Struct::ACodeNode* node;
};

} // namespace bloop

} // namespace banal

//*************************** Forward Declarations ***************************

#define DBG_CDS  0 /* debug coalesceDuplicateStmts */

static string OrphanedProcedureFile = Prof::Struct::Tree::UnknownFileNm;


//****************************************************************************
// Set of routines to write a structure tree
//****************************************************************************

// FIXME (minor): move to prof-juicy library for experiment writer
void
banal::bloop::writeStructure(std::ostream& os, Struct::Tree* strctTree, 
			     bool prettyPrint)
{
  static const char* structureDTD =
#include <lib/xml/hpc-structure.dtd.h>

  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE HPCToolkitStructure [\n" << structureDTD << "]>" << std::endl;
  os.flush();

  int oFlags = 0;
  if (!prettyPrint) { 
    oFlags |= Struct::Tree::OFlg_Compressed; 
  }
  
  os << "<HPCToolkitStructure i=\"0\" version=\"4.6\" n"
     << xml::MakeAttrStr(strctTree->name()) << ">\n";
  strctTree->writeXML(os, oFlags);
  os << "</HPCToolkitStructure>\n";
}

//****************************************************************************
// Set of routines to build a scope tree
//****************************************************************************

// makeStructure: Builds a scope tree -- with a focus on loop nest
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
Prof::Struct::LM*
banal::bloop::makeStructure(BinUtil::LM* lm, 
			    NormTy doNormalizeTy,
			    bool isIrrIvalLoop,
			    bool isFwdSubst,
			    ProcNameMgr* procNmMgr,
			    const std::string& dbgProcGlob)
{
  // Assume lm->Read() has been performed
  DIAG_Assert(lm, DIAG_UnexpectedInput);

  // FIXME (minor): relocate
  //OrphanedProcedureFile = Prof::Struct::Tree::UnknownFileNm + lm->name();

  Struct::LM* lmStrct = new Struct::LM(lm->name(), NULL);

  // 1. Build Struct::File/Struct::Proc skeletal structure
  ProcStrctToProcMap* mp = buildLMSkeleton(lmStrct, lm, procNmMgr);
  
  // 2. For each [Struct::Proc, BinUtil::Proc] pair, complete the build.
  // Note that a Struct::Proc may be associated with more than one
  // BinUtil::Proc.
  for (ProcStrctToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    Struct::Proc* pStrct = it->first;
    BinUtil::Proc* p = it->second;

    DIAG_Msg(2, "Building scope tree for [" << p->name()  << "] ... ");
    buildProcStructure(pStrct, p, isIrrIvalLoop, isFwdSubst, 
		       procNmMgr, dbgProcGlob);
  }
  delete mp;

  // 3. Normalize
  if (doNormalizeTy != NormTy_None) {
    bool doNormalizeUnsafe = (doNormalizeTy == NormTy_All);
    bool result = normalize(lmStrct, doNormalizeUnsafe);
    DIAG_Assert(result, "Normalization result should never be false!");
  }

  return lmStrct;
}


// normalize: Because of compiler optimizations and other things, it
// is almost always desirable normalize a scope tree.  For example,
// almost all unnormalized scope tree contain duplicate statement
// instances.  See each normalizing routine for more information.
bool 
banal::bloop::normalize(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe)
{
  bool changed = false;
  
  // Remove unnecessary alien scopes
  changed |= mergeBogusAlienStrct(lmStrct);

  // Cleanup procedure/alien scopes
  changed |= coalesceDuplicateStmts(lmStrct, doNormalizeUnsafe);
  changed |= mergePerfectlyNestedLoops(lmStrct);
  changed |= removeEmptyNodes(lmStrct);
  
  return true; // no errors
}


//****************************************************************************
// Helpers for building a scope tree
//****************************************************************************

namespace banal {

namespace bloop {

// buildLMSkeleton: Build skeletal file-procedure structure.  This
// will be useful later when detecting alien lines.  Also, the
// nesting structure allows inference of accurate boundaries on
// procedure end lines.
//
// A Struct::Proc can be associated with multiple BinUtil::Procs
//
// Struct::Procs will be sorted by begLn (cf. Struct::ACodeNode::Reorder)
static ProcStrctToProcMap*
buildLMSkeleton(Struct::LM* lmStrct, BinUtil::LM* lm, ProcNameMgr* procNmMgr)
{
  ProcStrctToProcMap* mp = new ProcStrctToProcMap;
  
  // -------------------------------------------------------
  // 1. Create basic structure for each procedure
  // -------------------------------------------------------

  for (BinUtil::LM::ProcMap::iterator it = lm->procs().begin();
       it != lm->procs().end(); ++it) {
    BinUtil::Proc* p = it->second;
    if (p->size() != 0) {
      Struct::File* fStrct = demandFileNode(lmStrct, p);
      Struct::Proc* pStrct = demandProcNode(fStrct, p, procNmMgr);
      mp->insert(make_pair(pStrct, p));
    }
  }

  // -------------------------------------------------------
  // 2. Establish nesting information:
  // -------------------------------------------------------
  for (ProcStrctToProcMap::iterator it = mp->begin(); it != mp->end(); ++it) {
    Struct::Proc* pStrct = it->first;
    BinUtil::Proc* p = it->second;
    BinUtil::Proc* parent = p->parent();

    if (parent) {
      Struct::Proc* parentScope = lmStrct->findProc(parent->begVMA());
      pStrct->Unlink();
      pStrct->Link(parentScope);
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


// demandFileNode: 
static Struct::File* 
demandFileNode(Struct::LM* lmStrct, BinUtil::Proc* p)
{
  // Attempt to find filename for procedure
  string filenm = p->filename();
  
  if (filenm.empty()) {
    string procnm;
    SrcFile::ln line;
    p->GetSourceFileInfo(p->begVMA(), 0, procnm, filenm, line);
  }
  if (filenm.empty()) { 
    filenm = OrphanedProcedureFile; 
  }
  
  // Obtain corresponding Struct::File
  return Struct::File::demand(lmStrct, filenm);
} 


// demandProcNode: Build skeletal Struct::Proc.  We can assume that
// the parent is always a Struct::File.
static Struct::Proc*
demandProcNode(Struct::File* fStrct, BinUtil::Proc* p, ProcNameMgr* procNmMgr)
{
  // Find VMA boundaries: [beg, end)
  VMA endVMA = p->begVMA() + p->size();
  VMAInterval bounds(p->begVMA(), endVMA);
  DIAG_Assert(!bounds.empty(), "Attempting to add empty procedure " 
	      << bounds.toString());
  
  // Find procedure name
  string procNm   = BinUtil::canonicalizeProcName(p->name(), procNmMgr);
  string procLnNm = BinUtil::canonicalizeProcName(p->linkName(), procNmMgr);
  
  // Find preliminary source line bounds
  string file, proc;
  SrcFile::ln begLn1, endLn1;
  BinUtil::Insn* eInsn = p->endInsn();
  ushort endOp = (eInsn) ? eInsn->opIndex() : 0;
  p->GetSourceFileInfo(p->begVMA(), 0, p->endVMA(), endOp, 
		       proc, file, begLn1, endLn1, 0 /*no swapping*/);
  
  // Compute source line bounds to uphold invariant:
  //   (begLn == 0) <==> (endLn == 0)
  SrcFile::ln begLn, endLn;
  if (p->hasSymbolic()) {
    begLn = p->begLine();
    endLn = std::max(begLn, endLn1);
  }
  else {
    // for now, always assume begLn to be more accurate
    begLn = begLn1;
    endLn = std::max(begLn1, endLn1);
  }

  // Obtain Struct::Proc by name.  This has the effect of fusing with
  // the existing Struct::Proc and accounts for procedure splitting or
  // specialization.
  bool didCreate = false;
  Struct::Proc* pStrct = Struct::Proc::demand(fStrct, procNm, procLnNm, 
					      begLn, endLn, &didCreate);
  if (!didCreate) {
    DIAG_DevMsg(3, "Merging multiple instances of procedure [" 
		<< pStrct->toStringXML() << "] with " << procNm << " " 
		<< procLnNm << " " << bounds.toString());
    pStrct->ExpandLineRange(begLn, endLn);
  }
  if (p->hasSymbolic()) {
    pStrct->hasSymbolic(p->hasSymbolic()); // optimistically set
  }
  pStrct->vmaSet().insert(bounds);
  
  return pStrct;
}

} // namespace bloop

} // namespace banal


//****************************************************************************
// 
//****************************************************************************

namespace banal {

namespace bloop {


static int 
buildProcLoopNests(Struct::Proc* enclosingProc, BinUtil::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::CFGInterface> cfg, 
		   OA::RIFG::NodeId fgRoot, 
		   bool isIrrIvalLoop, bool isFwdSubst,
		   ProcNameMgr* procNmMgr, bool isDbg);

static Struct::ACodeNode*
buildLoopAndStmts(bloop::LocationMgr& locMgr, 
		  Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::CFGInterface> cfg, 
		  OA::RIFG::NodeId fgNode,
		  bool isIrrIvalLoop, ProcNameMgr* procNmMgr);


// buildProcStructure: Complete the representation for 'pStrct' given the
// BinUtil::Proc 'p'.  Note that pStrcts parent may itself be a Struct::Proc.
static Struct::Proc* 
buildProcStructure(Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst, 
		   ProcNameMgr* procNmMgr, const std::string& dbgProcGlob)
{
  DIAG_Msg(3, "==> Proc `" << p->name() << "' (" << p->id() << ") <==");
  
  bool isDbg = false;
  if (!dbgProcGlob.empty()) {
    //uint dbgId = p->id();
    isDbg = FileUtil::fnmatch(dbgProcGlob, p->name().c_str());
  }
  
  buildProcLoopNests(pStrct, p, isIrrIvalLoop, isFwdSubst, procNmMgr, isDbg);
  
  return pStrct;
}


// buildProcLoopNests: Build procedure structure by traversing
// the Nested SCR (Tarjan tree) to create loop nests and statement
// scopes.
static int
buildProcLoopNests(Struct::Proc* pStrct, BinUtil::Proc* p,
		   bool isIrrIvalLoop, bool isFwdSubst, 
		   ProcNameMgr* procNmMgr, bool isDbg)
{
  static const int sepWidth = 77;
  using std::setfill;
  using std::setw;

  try {
    using banal::OAInterface;
    
    OA::OA_ptr<OAInterface> irIF; irIF = new OAInterface(p);
    
    OA::OA_ptr<OA::CFG::ManagerCFGStandard> cfgmanstd;
    cfgmanstd = new OA::CFG::ManagerCFGStandard(irIF);
    OA::OA_ptr<OA::CFG::CFG> cfg = 
      cfgmanstd->performAnalysis(TY_TO_IRHNDL(p, OA::ProcHandle));
    
    OA::OA_ptr<OA::RIFG> rifg; 
    rifg = new OA::RIFG(cfg, cfg->getEntry(), cfg->getExit());
    OA::OA_ptr<OA::NestedSCR> tarj; tarj = new OA::NestedSCR(rifg);
    
    OA::RIFG::NodeId fgRoot = rifg->getSource();

    if (isDbg) {
      cerr << setfill('=') << setw(sepWidth) << "=" << endl;
      cerr << "Procedure: " << p->name() << endl << endl;

      OA::OA_ptr<OA::OutputBuilder> ob1, ob2;
      ob1 = new OA::OutputBuilderText(cerr);
      ob2 = new OA::OutputBuilderDOT(cerr);

      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
      cerr << "*** CFG Text: [nodes, edges: " 
	   << cfg->getNumNodes() << ", " << cfg->getNumEdges() << "]\n";
      cfg->configOutput(ob1);
      cfg->output(*irIF);

      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
      cerr << "*** CFG DOT:\n";
      cfg->configOutput(ob2);
      cfg->output(*irIF);
      cerr << endl;

      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
      cerr << "*** Nested SCR (Tarjan) Tree\n";
      tarj->dump(cerr);
      cerr << endl;

      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
      cerr << endl << flush;
    }

    int r = buildProcLoopNests(pStrct, p, tarj, cfg, fgRoot,
			       isIrrIvalLoop, isFwdSubst, procNmMgr, isDbg);

    if (isDbg) {
      cerr << setfill('-') << setw(sepWidth) << "-" << endl;
    }

    return r;
  }
  catch (const OA::Exception& x) {
    std::ostringstream os;
    x.report(os);
    DIAG_Throw("[OpenAnalysis] " << os.str());
  }
}


// buildProcLoopNests: Visit the object code loops in pre-order and
// create source code representations.  Technically we choose to visit
// in BFS order, which in a better world would would allow us to check
// sibling loop boundaries.  Note that the resulting source coded
// loops are UNNORMALIZED.
static int 
buildProcLoopNests(Struct::Proc* enclosingProc, BinUtil::Proc* p,
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::CFGInterface> cfg, 
		   OA::RIFG::NodeId fgRoot, 
		   bool isIrrIvalLoop, bool isFwdSubst, 
		   ProcNameMgr* procNmMgr, bool isDbg)
{
  typedef std::list<QNode> MyQueue;

  int nLoops = 0;

  std::vector<bool> isNodeProcessed(tarj->getRIFG()->getHighWaterMarkNodeId() + 1);
  
  bloop::LocationMgr locMgr(enclosingProc->AncLM());
  if (isDbg) {
    locMgr.debug(1);
  }

  locMgr.begSeq(enclosingProc, isFwdSubst);
  
  // -------------------------------------------------------
  // Process the Nested SCR (Tarjan tree) in preorder
  // -------------------------------------------------------

  // NOTE: The reason for this generality is that we experimented with
  // a "split" BFS search in a futile attempt to recover the inlining
  // tree.

  // Queue INVARIANTs.  The queue has two sections to support general searches:
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
      Struct::ACodeNode* myScope;
      myScope = buildLoopAndStmts(locMgr, qnode.enclosingStrct, p,
				  tarj, cfg, qnode.fgNode, isIrrIvalLoop, 
				  procNmMgr);
      isNodeProcessed[qnode.fgNode] = true;
      qnode.scope = myScope;
      if (myScope->type() == Struct::ANode::TyLOOP) {
	nLoops++;
      }
    }
    
    // -------------------------------------------------------
    // 3. process children within this context in BFS fashion
    //    (Note: WasCtxtClosed() always false -> BFS)
    // -------------------------------------------------------
    OA::RIFG::NodeId kid = tarj->getInners(qnode.fgNode);
    if (kid == OA::RIFG::NIL) {
      continue;
    }
    
    do {
      Struct::ACodeNode* myScope;
      myScope = buildLoopAndStmts(locMgr, qnode.scope, p, 
				  tarj, cfg, kid, isIrrIvalLoop, procNmMgr);
      isNodeProcessed[kid] = true;
      if (myScope->type() == Struct::ANode::TyLOOP) {
	nLoops++;
      }
      
      // Insert to BFS section (inserts before)
      theQueue.insert(q_todo, QNode(kid, qnode.scope, myScope));
      kid = tarj->getNext(kid);
    }
    while (kid != OA::RIFG::NIL /* && !WasCtxtClosed(qnode.scope, locMgr) */);

    // NOTE: *If* we knew the loop was correctly parented in a
    // procedure context, we could check sibling boundaries.  However,
    // there are cases where an initial loop nesting must be
    // corrected.

    // -------------------------------------------------------
    // 4. place rest of children in queue's TODO section
    // -------------------------------------------------------
    for ( ; kid != OA::RIFG::NIL; kid = tarj->getNext(kid)) {
      theQueue.push_back(QNode(kid, qnode.scope, NULL));

      // ensure 'q_todo' points to the beginning of TODO section
      if (q_todo == theQueue.end()) {
	q_todo--;
      }
    }
  }

  // -------------------------------------------------------
  // Process nodes that we have not visited.
  //
  // This may occur if the CFG is disconnected.  E.g., we do not
  // correctly handle jump tables.  Note that we cannot be sure of the
  // location of these statements within procedure structure.
  // -------------------------------------------------------

  for (uint i = 1; i < isNodeProcessed.size(); ++i) {
    if (!isNodeProcessed[i]) {
      // INVARIANT: return value is never a Struct::Loop
      buildLoopAndStmts(locMgr, enclosingProc, p, tarj, cfg, i, isIrrIvalLoop,
			procNmMgr);
    }
  }

  
  locMgr.endSeq();

  return nLoops;
}


// buildLoopAndStmts: Returns the expected (or 'original') enclosing
// scope for children of 'fgNode', e.g. loop or proc. 
static Struct::ACodeNode*
buildLoopAndStmts(bloop::LocationMgr& locMgr, 
		  Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::CFGInterface> cfg, 
		  OA::RIFG::NodeId fgNode, 
		  bool isIrrIvalLoop, ProcNameMgr* procNmMgr)
{
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::NodeInterface> bb = 
    rifg->getNode(fgNode).convert<OA::CFG::NodeInterface>();
  BinUtil::Insn* insn = banal::OA_CFG_getBegInsn(bb);
  VMA begVMA = (insn) ? insn->opVMA() : 0;
  
  DIAG_DevMsg(10, "buildLoopAndStmts: " << bb << " [id: " << bb->getId() << "] " << hex << begVMA << " --> " << enclosingStrct << dec << " " << enclosingStrct->toString_id());

  Struct::ACodeNode* childScope = enclosingStrct;

  OA::NestedSCR::Node_t ity = tarj->getNodeType(fgNode);
  if (ity == OA::NestedSCR::NODE_ACYCLIC 
      || ity == OA::NestedSCR::NODE_NOTHING) {
    // -----------------------------------------------------
    // ACYCLIC: No loops
    // -----------------------------------------------------
  }
  else if (ity == OA::NestedSCR::NODE_INTERVAL || 
	   (isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) {
    // -----------------------------------------------------
    // INTERVAL or IRREDUCIBLE as a loop: Loop head
    // -----------------------------------------------------
    //Struct::ACodeNode* procCtxt = enclosingStrct->ancestorProcCtxt();
    string fnm, pnm;
    SrcFile::ln line;
    findLoopBegLineInfo(/*procCtxt,*/ p, bb, fnm, pnm, line);
    pnm = BinUtil::canonicalizeProcName(pnm, procNmMgr);
    
    Struct::Loop* loop = new Struct::Loop(NULL, line, line);
    loop->vmaSet().insert(begVMA, begVMA + 1); // a loop id
    locMgr.locate(loop, enclosingStrct, fnm, pnm, line);
    childScope = loop;
  }
  else if (!isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
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
  buildStmts(locMgr, childScope, p, bb, procNmMgr);

  return childScope;
}


// buildStmts: Form loop structure by setting bounds and adding
// statements from the current basic block to 'enclosingStrct'.  Does
// not add duplicates.
static int 
buildStmts(bloop::LocationMgr& locMgr,
	   Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p,
	   OA::OA_ptr<OA::CFG::NodeInterface> bb, ProcNameMgr* procNmMgr)
{
  static int call_sortId = 0;

  OA::OA_ptr<OA::CFG::NodeStatementsIteratorInterface> it =
    bb->getNodeStatementsIterator();
  for ( ; it->isValid(); ) {
    BinUtil::Insn* insn = IRHNDL_TO_TY(it->current(), BinUtil::Insn*);
    VMA vma = insn->vma();
    ushort opIdx = insn->opIndex();
    VMA opvma = insn->opVMA();
    
    // advance iterator [needed when creating VMA interval]
    ++(*it);

    // 1. gather source code info
    string filenm, procnm;
    SrcFile::ln line;
    p->GetSourceFileInfo(vma, opIdx, procnm, filenm, line); 
    procnm = BinUtil::canonicalizeProcName(procnm, procNmMgr);

    // 2. create a VMA interval
    // the next (or hypothetically next) insn begins no earlier than:
    BinUtil::Insn* n_insn = (it->isValid()) ? 
      IRHNDL_TO_TY(it->current(), BinUtil::Insn*) : NULL;
    VMA n_opvma = (n_insn) ? n_insn->opVMA() : insn->endVMA();
    DIAG_Assert(opvma < n_opvma, "Invalid VMAInterval: [" << opvma << ", "
		<< n_opvma << ")");

    VMAInterval vmaint(opvma, n_opvma);

    // 3. Get statement type.  Use a special sort key for calls as a
    // way to protect against bad debugging information which would
    // later cause a call to treated as loop-invariant-code motion and
    // hoisted into a different loop.
    ISA::InsnDesc idesc = insn->desc();
    
    // 4. locate stmt
    Struct::Stmt* stmt = 
      new Struct::Stmt(NULL, line, line, vmaint.beg(), vmaint.end());
    if (idesc.IsSubr()) {
      stmt->sortId(--call_sortId);
    }
    locMgr.locate(stmt, enclosingStrct, filenm, procnm, line);
  }
  return 0;
} 


#if 0 // FIXME: Deprecated
// buildProcLoopNests: Recursively build loops using Nested SCR
// (Tarjan interval) analysis and returns the number of loops created.
// 
// We visit the interval tree in DFS order which is equivalent to
// visiting each basic block in VMA order.
//
// Note that these loops are UNNORMALIZED.
static int 
buildProcLoopNests(Struct::ACodeNode* enclosingStrct, BinUtil::Proc* p, 
		   OA::OA_ptr<OA::NestedSCR> tarj,
		   OA::OA_ptr<OA::CFG::Interface> cfg, 
		   OA::RIFG::NodeId fgNode, 
		   int addStmts, bool isIrrIvalLoop)
{
  int localLoops = 0;
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::Interface::Node> bb =
    rifg->getNode(fgNode).convert<OA::CFG::Interface::Node>();
  
  DIAG_DevMsg(50, "buildProcLoopNests: " << bb <<  " --> "  << hex << enclosingStrct << dec << " " << enclosingStrct->toString_id());

  if (addStmts) {
    // mp->push_back(make_pair(bb, enclosingStrct));
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
	//mp->push_back(make_pair(bb1, enclosingStrct));
      }
    }
    else if (ity == OA::NestedSCR::NODE_INTERVAL || 
	     (isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) {
      // -----------------------------------------------------
      // INTERVAL or IRREDUCIBLE as a loop: Loop head
      // -----------------------------------------------------

      // add alien context if necessary....
      Struct::Loop* lScope = new Struct::Loop(enclosingStrct, line, line);
      int num = buildProcLoopNests(lScope, p, tarj, cfg, kid, mp,
				   1, isIrrIvalLoop);
      localLoops += (num + 1);
    }
    else if (!isIrrIvalLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
      // -----------------------------------------------------
      // IRREDUCIBLE as no loop: May contain loops
      // -----------------------------------------------------
      int num = buildProcLoopNests(enclosingStrct, p, tarj, cfg, kid, mp,
				   addStmts, isIrrIvalLoop);
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

// findLoopBegLineInfo: Given the head basic block node of the loop,
// find loop begin source line information.  
//
// The routine first attempts to use the source line information for
// the backward branch, if one exists.  Then, it consults the 'head'
// instruction.
//
// Note: It is possible to have multiple or no backward branches
// (e.g. an 'unstructured' loop written with IFs and GOTOs).  In the
// former case, we take the smallest source line of them all; in the
// latter we use headVMA.
static void
findLoopBegLineInfo(/* Struct::ACodeNode* procCtxt,*/ BinUtil::Proc* p,
		    OA::OA_ptr<OA::CFG::NodeInterface> headBB,
		    string& begFileNm, string& begProcNm, SrcFile::ln& begLn)
{
  using namespace OA::CFG;

  begLn = SrcFile::ln_NULL;

  // Find the head vma
  BinUtil::Insn* head = banal::OA_CFG_getBegInsn(headBB);
  VMA headVMA = head->vma();
  ushort headOpIdx = head->opIndex();
  DIAG_Assert(headOpIdx == 0, "Target of a branch at " << headVMA 
	      << " has op-index of: " << headOpIdx);
  
  // ------------------------------------------------------------
  // Attempt to use backward branch to find loop begin line (see note above)
  // ------------------------------------------------------------
  OA::OA_ptr<EdgesIteratorInterface> it 
    = headBB->getCFGIncomingEdgesIterator();
  for ( ; it->isValid(); ++(*it)) {
    OA::OA_ptr<EdgeInterface> e = it->currentCFGEdge();
    
    OA::OA_ptr<NodeInterface> bb = e->getCFGSource();

    BinUtil::Insn* backBR = banal::OA_CFG_getEndInsn(bb);
    if (!backBR) {
      continue;
    }
    
    VMA vma = backBR->vma();
    ushort opIdx = backBR->opIndex();

    // If we have a backward edge, find the source line of the
    // backward branch.  Note: back edges are not always labeled as such!
    if (e->getType() == BACK_EDGE || vma >= headVMA) {
      SrcFile::ln line;
      string filenm, procnm;
      p->GetSourceFileInfo(vma, opIdx, procnm, filenm, line); 
      if (SrcFile::isValid(line) 
	  && (!SrcFile::isValid(begLn) || line < begLn)) {
	begLn = line;
	begFileNm = filenm;
	begProcNm = procnm;
      }
    }
  }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------  
  if (!SrcFile::isValid(begLn)) {
    // Fall through: consult the first instruction in the loop
    p->GetSourceFileInfo(headVMA, headOpIdx, begProcNm, begFileNm, begLn);
  }

#if 0 
  // TODO: Possibly try to have two levels of forward-substitution-off
  // support.  The less agressive level (this code) compares the first
  // instruction in a loop with the loop backward branch in an attempt
  // to arrive at a better loop begin line (which is only adjusted by
  // fuzzy matching or by self-nesting correction).  The more
  // aggressive level is the min-max loop heuristic and is implemented
  // in LocationMgr::determineContext().

  else if (/* forward substitution off */) {
    // INVARIANT: backward branch was found (begLn is valid)
    
    // If head instruction appears to come from the same procedure
    // context as the backward branch, then compare the two sets of
    // source information.
    SrcFile::ln line;
    string filenm, procnm;
    p->GetSourceFileInfo(headVMA, headOpIdx, procnm, filenm, line);
    
    if (filenm == begFileNm && ctxtNameEqFuzzy(procnm, begProcNm)
	&& procCtxt->begLine() < line && line < procCtxt->endLine()) {
      begLn = line;
      begFileNm = filenm;
      begProcNm = procnm;
    }
  }
#endif
}



#if 0
// demandStmtNode: Build a Struct::Stmt.  Unlike LocateStmt,
// assumes that procedure boundaries do not need to be expanded.
static Struct::Stmt*
demandStmtNode(std::map<SrcFile::ln, Struct::Stmt*>& stmtMap,
	       Struct::ACodeNode* enclosingStrct, SrcFile::ln line, 
	       VMAInterval& vmaint)
{
  Struct::Stmt* stmt = stmtMap[line];
  if (!stmt) {
    stmt = new Struct::Stmt(enclosingStrct, line, line, 
			    vmaint.beg(), vmaint.end());
    stmtMap.insert(make_pair(line, stmt));
  }
  else {
    stmt->vmaSet().insert(vmaint); // expand VMA range
  }
  return stmt;
}
#endif

} // namespace bloop

} // namespace banal


//****************************************************************************
// Helpers for normalizing a scope tree
//****************************************************************************

namespace banal {

namespace bloop {


static bool 
mergeBogusAlienStrct(Struct::ACodeNode* node, Struct::File* file);


static bool 
mergeBogusAlienStrct(Prof::Struct::LM* lmStrct)
{
  bool changed = false;
    
  for (Struct::ANodeIterator it(lmStrct, 
				&Struct::ANodeTyFilter[Struct::ANode::TyPROC]);
       it.Current(); ++it) {
    Struct::Proc* proc = dynamic_cast<Struct::Proc*>(it.Current());
    Struct::File* file = proc->AncFile();
    changed |= mergeBogusAlienStrct(proc, file);
  }
  
  return changed;
}


static bool 
mergeBogusAlienStrct(Struct::ACodeNode* node, Struct::File* file)
{
  bool changed = false;
  
  if (!node) { return changed; }

  for (Struct::ACodeNodeChildIterator it(node); it.Current(); /* */) {
    Struct::ACodeNode* child = it.CurNode();
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    changed |= mergeBogusAlienStrct(child, file);
    
    // 2. Merge an alien node if it is redundant with its procedure context
    if (child->type() == Struct::ANode::TyALIEN) {
      Struct::Alien* alien = dynamic_cast<Struct::Alien*>(child);
      Struct::ACodeNode* parent = alien->ACodeNodeParent();
      
      Struct::ACodeNode* procCtxt = parent->ancestorProcCtxt();
      const string& procCtxtFnm = (procCtxt->type() == Struct::ANode::TyALIEN) ?
	dynamic_cast<Struct::Alien*>(procCtxt)->fileName() : file->name();
      
      // FIXME: Looking at this again, don't we know that 'procCtxtFnm' is alien?
      if (alien->fileName() == procCtxtFnm
	  && ctxtNameEqFuzzy(procCtxt, alien->name())
	  && LocationMgr::containsIntervalFzy(parent, alien->begLine(), 
					      alien->endLine()))  {
	// Move all children of 'alien' into 'parent'
	changed = Struct::ANode::Merge(parent, alien);
	DIAG_Assert(changed, "mergeBogusAlienStrct: merge failed.");
      }
    }
  }
  
  return changed;
}


//****************************************************************************

// coalesceDuplicateStmts: Coalesce duplicate statement instances that
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
//   multiple basic blocks containing the same statement, cf. buildStmts(). 
//   [FIXME -- LocationMgr]
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
static bool CDS_doNormalizeUnsafe = true;

static bool
coalesceDuplicateStmts(Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap, 
		       Struct::ANodeSet* visited, Struct::ANodeSet* toDelete,
		       int level);

static bool
CDS_Main(Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap, 
	 Struct::ANodeSet* visited, Struct::ANodeSet* toDelete, int level);

static bool
CDS_InspectStmt(Struct::Stmt* stmt1, SortIdToStmtMap* stmtMap, 
		Struct::ANodeSet* toDelete, int level);

static bool 
CDS_ScopeFilter(const Struct::ANode& x, long type)
{
  return (x.type() == Struct::ANode::TyPROC 
	  || x.type() == Struct::ANode::TyALIEN);
}

static bool
coalesceDuplicateStmts(Prof::Struct::LM* lmStrct, bool doNormalizeUnsafe)
{
  bool changed = false;
  CDS_doNormalizeUnsafe = doNormalizeUnsafe;
  SortIdToStmtMap stmtMap;    // line to statement data map
  Struct::ANodeSet visitedScopes; // all children of a scope have been visited
  Struct::ANodeSet toDelete;      // nodes to delete

  // Apply the normalization routine to each Struct::Proc and Struct::Alien
  // so that 1) we are guaranteed to only process Struct::ACodeNodes and 2) we
  // can assume that all line numbers encountered are within the same
  // file (keeping the SortIdToStmtMap simple and fast).  (Children
  // Struct::Alien's are skipped.)

  Struct::ANodeFilter filter(CDS_ScopeFilter, "CDS_ScopeFilter", 0);
  for (Struct::ANodeIterator it(lmStrct, &filter); it.Current(); ++it) {
    Struct::ACodeNode* scope = dynamic_cast<Struct::ACodeNode*>(it.Current());
    changed |= coalesceDuplicateStmts(scope, &stmtMap, &visitedScopes,
				      &toDelete, 1);
    stmtMap.clear();           // Clear statement table
    visitedScopes.clear();     // Clear visited set
    deleteContents(&toDelete); // Clear 'toDelete'
  }

  return changed;
}


// coalesceDuplicateStmts Helper: 
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
// - Note that the merging of Struct::ACodeNodes can trigger a reordering based
//   on begin/end lines; new children will not simply end up at the
//   end of the child list.
//
// Another solution: Divide into two distinct phases.  Phase 1 collects all
//   statements into a multi-map (that handles sorted iteration and
//   fast local resorts).  Phase 2 iterates over the map, applying
//   case 1 and 2 until all duplicate entries are removed.
static bool
coalesceDuplicateStmts(Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap, 
		       Struct::ANodeSet* visited, Struct::ANodeSet* toDelete, 
		       int level)
{
  try {
    return CDS_Main(scope, stmtMap, visited, toDelete, level);
  } 
  catch (CDS_RestartException& x) {
    // Unwind the recursion stack until we find the node
    if (x.GetNode() == scope) {
      return coalesceDuplicateStmts(x.GetNode(), stmtMap, visited, 
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
CDS_Main(Struct::ACodeNode* scope, SortIdToStmtMap* stmtMap, Struct::ANodeSet* visited, 
	 Struct::ANodeSet* toDelete, int level)
{
  bool changed = false;
  
  if (!scope) { return changed; }
  if (visited->find(scope) != visited->end()) { return changed; }
  if (toDelete->find(scope) != toDelete->end()) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (Struct::ANodeChildIterator it(scope); it.Current(); ++it) {
    Struct::ACodeNode* child = dynamic_cast<Struct::ACodeNode*>(it.Current()); // always true
    DIAG_Assert(child, "");
    
    if (toDelete->find(child) != toDelete->end()) { continue; }
    
    if (child->type() == Struct::ANode::TyALIEN) { continue; }
    
    DIAG_DevMsgIf(DBG_CDS, "CDS: " << child);

    // 1. Recursively perform re-nesting on 'child'.
    changed |= coalesceDuplicateStmts(child, stmtMap, visited, toDelete,
				      level + 1);
    
    // 2. Examine 'child'
    if (child->type() == Struct::ANode::TySTMT) {
      // Note: 'child' may be deleted or a restart exception may be thrown
      Struct::Stmt* stmt = dynamic_cast<Struct::Stmt*>(child);
      changed |= CDS_InspectStmt(stmt, stmtMap, toDelete, level);
    } 
  }
  
  visited->insert(scope);
  return changed; 
}

  
// CDS_InspectStmt: applies case 1 or 2, as described above
static bool
CDS_InspectStmt(Struct::Stmt* stmt1, SortIdToStmtMap* stmtMap, 
		Struct::ANodeSet* toDelete, int level)
{
  bool changed = false;
  
  int sortid = stmt1->sortId();
  StmtData* stmtdata = (*stmtMap)[sortid];
  if (stmtdata) {
    
    Struct::Stmt* stmt2 = stmtdata->GetStmt();
    DIAG_DevMsgIf(DBG_CDS, " Find: " << stmt1 << " " << stmt2);
    
    // Ensure we have two different instances of the same sortid (line)
    if (stmt1 == stmt2) { return false; }
    
    // Find the least common ancestor.  At most it should be a
    // procedure scope.
    Struct::ANode* lca = Struct::ANode::LeastCommonAncestor(stmt1, stmt2);
    DIAG_Assert(lca, "");
    
    // Because we have the lca and know that the descendent nodes are
    // statements (leafs), the test for case 1 is very simple:
    bool case1 = (stmt1->Parent() == lca || stmt2->Parent() == lca);
    if (case1) {
      // Case 1: Duplicate statements. Delete shallower one.
      Struct::Stmt* toRemove = NULL;
      
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
    else if (CDS_doNormalizeUnsafe) {
      // Case 2: Duplicate statements in different loops (or scopes).
      // Merge the nodes from stmt2->lca into those from stmt1->lca.
      DIAG_DevMsgIf(DBG_CDS, "  Merge: " << stmt1 << " <- " << stmt2);
      changed = Struct::ANode::MergePaths(lca, stmt1, stmt2);
      if (changed) {
	// We may have created instances of case 1.  Furthermore,
	// while neither statement is deleted ('stmtMap' is still
	// valid), iterators between stmt1 and 'lca' are invalidated.
	// Restart at lca.
	Struct::ACodeNode* lca_CI = dynamic_cast<Struct::ACodeNode*>(lca);
	DIAG_Assert(lca_CI, "");
	throw CDS_RestartException(lca_CI);
      }
    }
  } 
  else {
    // Add the statement instance to the map
    stmtdata = new StmtData(stmt1, level);
    (*stmtMap)[sortid] = stmtdata;
    DIAG_DevMsgIf(DBG_CDS, " Map: " << stmt1);
  }
  
  return changed;
}


//****************************************************************************

// mergePerfectlyNestedLoops: Fuse together a child with a parent when
// the child is perfectly nested in the parent.
static bool 
mergePerfectlyNestedLoops(Struct::ANode* node)
{
  bool changed = false;
  
  if (!node) { return changed; }
  
  // A post-order traversal of this node (visit children before parent)...
  for (Struct::ANodeChildIterator it(node); it.Current(); /* */) {
    Struct::ACodeNode* child = dynamic_cast<Struct::ACodeNode*>(it.Current()); // always true
    DIAG_Assert(child, "");
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    changed |= mergePerfectlyNestedLoops(child);
    
    // 2. Test if this is a perfectly nested loop with identical loop
    //    bounds and merge if so.

    // The following cast may be illegal but the 'perfect nesting' test
    //   will ensure it doesn't cause a problem (Loops are Struct::ACodeNode's).
    // Perfectly nested test: child is a loop; parent is a loop; and
    //   this is only child.  
    Struct::ACodeNode* n_CI = dynamic_cast<Struct::ACodeNode*>(node); 
    bool perfNested = (child->type() == Struct::ANode::TyLOOP &&
		       node->type() == Struct::ANode::TyLOOP &&
		       node->ChildCount() == 1);
    if (perfNested && SrcFile::isValid(child->begLine(), child->endLine()) &&
	child->begLine() == n_CI->begLine() &&
	child->endLine() == n_CI->endLine()) { 

      // Move all children of 'child' so that they are children of 'node'
      changed = Struct::ANode::Merge(node, child);
      DIAG_Assert(changed, "mergePerfectlyNestedLoops: merge failed.");
    }
  } 
  
  return changed; 
}


//****************************************************************************

// removeEmptyNodes: Removes certain 'empty' scopes from the tree,
// always maintaining the top level Struct::Root (PGM) scope.  See the
// predicate 'removeEmptyNodes_isEmpty' for details.
static bool 
removeEmptyNodes_isEmpty(const Struct::ANode* node);

static bool 
removeEmptyNodes(Struct::ANode* node)
{
  bool changed = false;
  
  if (!node) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (Struct::ANodeChildIterator it(node); it.Current(); /* */) {
    Struct::ANode* child = dynamic_cast<Struct::ANode*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any trimming for this tree's children
    changed |= removeEmptyNodes(child);

    // 2. Trim this node if necessary
    if (removeEmptyNodes_isEmpty(child)) {
      child->Unlink(); // unlink 'child' from tree
      delete child;
      changed = true;
    }
  } 
  
  return changed; 
}


// removeEmptyNodes_isEmpty: determines whether a scope is 'empty':
//   true, if a Struct::File has no children.
//   true, if a Struct::Loop or Struct::Proc has no children *and* an empty
//     VMAIntervalSet.
//   false, otherwise
static bool 
removeEmptyNodes_isEmpty(const Struct::ANode* node)
{
  if ((node->type() == Struct::ANode::TyFILE 
       || node->type() == Struct::ANode::TyALIEN)
      && node->ChildCount() == 0) {
    return true;
  }
  if ((node->type() == Struct::ANode::TyPROC 
       || node->type() == Struct::ANode::TyLOOP)
      && node->ChildCount() == 0) {
    const Struct::ACodeNode* n = dynamic_cast<const Struct::ACodeNode*>(node);
    return n->vmaSet().empty();
  }
  return false;
}


//****************************************************************************
// Helpers for traversing the Nested SCR (Tarjan Tree)
//****************************************************************************

void
SortIdToStmtMap::clear()
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
deleteContents(Struct::ANodeSet* s)
{
  // Delete nodes in toDelete
  for (Struct::ANodeSet::iterator it = s->begin(); it != s->end(); ++it) {
    Struct::ANode* n = (*it);
    n->Unlink(); // unlink 'n' from tree
    delete n;
  }
  s->clear();
}


} // namespace bloop

} // namespace banal

