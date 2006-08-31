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

#include <map> // STL

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/CFG/ManagerCFGStandard.hpp>
#include <OpenAnalysis/Utils/RIFG.hpp>
#include <OpenAnalysis/Utils/NestedSCR.hpp>
#include <OpenAnalysis/Utils/Exception.hpp>

//*************************** User Include Files ****************************

#include "bloop.hpp"
#include "OAInterface.hpp"

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/BinUtils.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Files.hpp>
#include <lib/support/pathfind.h>

//*************************** Forward Declarations ***************************

typedef std::multimap<ProcScope*, binutils::Proc*> ProcScopeToProcMap;

// Helpers for building a scope tree

static ProcScopeToProcMap*
BuildStructure(LoadModScope *lmScope, binutils::LM* lm);

static ProcScope*
BuildProcStructure(FileScope* fileScope, binutils::Proc* p);


static ProcScope*
BuildFromProc(ProcScope* pScope, binutils::Proc* p, 
	      bool irreducibleIntervalIsLoop); 

static int
BuildFromTarjTree(ProcScope* pScope, binutils::Proc* p, 
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::Interface> cfg, 
		  OA::RIFG::NodeId fgNode,
		  bool irreducibleIntervalIsLoop);

static int
BuildFromBB(CodeInfo* enclosingScope, binutils::Proc* p, 
	    OA::OA_ptr<OA::CFG::Interface::Node> bb);

static FileScope*
FindOrCreateFileNode(LoadModScope* lmScope, binutils::Proc* p);

static suint 
FindLoopBegLine(binutils::Proc* p, OA::OA_ptr<OA::CFG::Interface::Node> bb);

//*************************** Forward Declarations ***************************

// Helpers for normalizing a scope tree

static bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree);

static bool
CoalesceDuplicateStmts(PgmScopeTree* pgmScopeTree, bool unsafeNormalizations);

static bool 
MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree);

static bool 
RemoveEmptyScopes(PgmScopeTree* pgmScopeTree);

static bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, 
			 const char* canonicalPathList);

//*************************** Forward Declarations ***************************

// ------------------------------------------------------------
// Helpers for traversing the Nested SCR (Tarjan Tree)
// ------------------------------------------------------------

// CFGNode -> <begVMA, endVMA>
class CFGNodeToVMAMap 
  : public std::map<OA::OA_ptr<OA::CFG::Interface::Node>, 
		    std::pair<VMA, VMA>* > 
{
public:
  typedef std::map<OA::OA_ptr<OA::CFG::Interface::Node>, 
		   std::pair<VMA, VMA>* > My_t;

public:
  CFGNodeToVMAMap() { }
  CFGNodeToVMAMap(OA::OA_ptr<OA::CFG::Interface> cfg, binutils::Proc* p) 
    { build(cfg, cfg->getEntry(), p, 0); }
  virtual ~CFGNodeToVMAMap() { clear(); }
  virtual void clear();

  void build(OA::OA_ptr<OA::CFG::Interface> cfg, 
	     OA::OA_ptr<OA::CFG::Interface::Node> n, 
	     binutils::Proc* p, VMA _end);
};

static void 
CFG_GetBegAndEndVMAs(OA::OA_ptr<OA::CFG::Interface::Node> n, 
		     VMA &beg, VMA &end);


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

//*************************** Forward Declarations ***************************

#define DBG_PROC 0 /* debug BuildFromProc */
#define DBG_CDS  0 /* debug CoalesceDuplicateStmts */

//#define BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES

static string OrphanedProcedureFile =
  "~~~<unknown-file>~~~";

static const char *PGMdtd =
#include <lib/xml/PGM.dtd.h>

//****************************************************************************
// Set of routines to write a scope tree
//****************************************************************************

// FIXME: move to prof-juicy library for experiment writer
void
banal::WriteScopeTree(std::ostream& os, PgmScopeTree* pgmScopeTree, 
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

// BuildFromLM: Builds a scope tree -- with a focus on loop nest
//   recovery -- representing program source code from the load module
// '  lm'.
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
banal::BuildFromLM(binutils::LM* lm, 
		   const char* canonicalPathList, 
		   bool normalizeScopeTree,
		   bool unsafeNormalizations,
		   bool irreducibleIntervalIsLoop,
		   bool verboseMode)
{
  DIAG_Assert(lm, DIAG_UnexpectedInput);

  PgmScopeTree* pgmScopeTree = NULL;
  PgmScope* pgmScope = NULL;

  // FIXME: relocate
  OrphanedProcedureFile = "~~~" + lm->GetName() + ":<unknown-file>~~~";

  // Assume lm->Read() has been performed
  pgmScope = new PgmScope("");
  pgmScopeTree = new PgmScopeTree("", pgmScope);

  LoadModScope* lmScope = new LoadModScope(lm->GetName(), pgmScope);

  // 1. Build basic FileScope/ProcScope structure
  ProcScopeToProcMap* pmap = BuildStructure(lmScope, lm);
  
  // 2. For each [ProcScope, binutils::Proc] pair, complete the build.
  // Note that a ProcScope may be associated with more than one
  // binutils::Proc.
  for (ProcScopeToProcMap::iterator it = pmap->begin(); 
       it != pmap->end(); ++it) {
    ProcScope* pScope = it->first;
    binutils::Proc* p = it->second;

    if (verboseMode) {
      cerr << "Building scope tree for [" << p->GetName()  << "] ... ";
    }
    BuildFromProc(pScope, p, irreducibleIntervalIsLoop);
    if (verboseMode) {
      cerr << "done " << endl;
    }
  }
  delete pmap;

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
banal::Normalize(PgmScopeTree* pgmScopeTree, 
			    bool unsafeNormalizations)
{
  bool changed = false;
  // changed |= RemoveOrphanedProcedureRepository(pgmScopeTree);
  
#if 0 /* not necessary */
  // Apply following routines until a fixed point is reached.
  do {
#endif
    changed = false;
    changed |= CoalesceDuplicateStmts(pgmScopeTree, unsafeNormalizations);
#if 0
  } while (changed);
#endif
  
  changed |= MergePerfectlyNestedLoops(pgmScopeTree);
  changed |= RemoveEmptyScopes(pgmScopeTree);
  
  return true; // no errors
}


//****************************************************************************
// Helpers for building a scope tree
//****************************************************************************

// BuildStructure: Build basic file-procedure structure.  This
// will be useful later when relocating alien lines.  Also, the
// nesting structure allows inference of accurate boundaries on
// procedure end lines.
//
// A ProcScope can be associated with multiple binutils::Procs
static ProcScopeToProcMap*
BuildStructure(LoadModScope* lmScope, binutils::LM* lm)
{
  ProcScopeToProcMap* mp = new ProcScopeToProcMap;
  
  // -------------------------------------------------------
  // 1. Create structure for each procedure
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
      FileScope* fileScope = FindOrCreateFileNode(lmScope, p);
      ProcScope* procScope = BuildProcStructure(fileScope, p);
      mp->insert(make_pair(procScope, p));
    }
  }

  // -------------------------------------------------------
  // 2. Establish nesting information:
  //      if we don't have parent information
  //        nest using source line bounds (FIXME)
  //      otherwise, nest using parent information
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
  
  // 3. Establish procedure bounds: nesting + first line ... FIXME
  
  return mp;
}


// BuildProcStructure: Build skeletal ProcScope.  We can assume that
// the parent is always a FileScope.
static ProcScope*
BuildProcStructure(FileScope* fileScope, binutils::Proc* p)
{
  // Find VMA boundaries: [beg, end)
  VMA endVMA = p->GetBegVMA() + p->GetSize();
  VMAInterval bounds(p->GetBegVMA(), endVMA);
  DIAG_Assert(!bounds.empty(), "Attempting to add empty procedure " 
	      << bounds.toString());
  
  // Find procedure name
  string funcNm   = GetBestFuncName(p->GetName()); 
  string funcLnNm = GetBestFuncName(p->GetLinkName());
  
  // Find preliminary source line bounds
  string func, file;
  suint begLn1, endLn1;
  binutils::Insn* eInsn = p->GetLastInsn();
  ushort endOp = (eInsn) ? eInsn->GetOpIndex() : 0;
  p->GetSourceFileInfo(p->GetBegVMA(), 0, p->GetEndVMA(), endOp,
		       func, file, begLn1, endLn1);

  suint begLn, endLn;
  if (p->hasSymbolic()) {
    begLn = p->GetBegLine();
    endLn = (endLn1 >= begLn) ? endLn1 : begLn;
  }
  else {
    begLn = begLn1;
    endLn = endLn1;
  }
  
  // Create or find the scope.  Fuse procedures if we names match.
  ProcScope* pScope = fileScope->FindProc(funcNm, funcLnNm);
  
  if (!pScope) {
    pScope = new ProcScope(funcNm, fileScope, funcLnNm, begLn, endLn);
  }
  else {
    // Assume the procedure was split.  Fuse it together.
    DIAG_DevMsg(3, "Merging multiple instances of procedure [" << pScope->toStringXML() << "] with " << funcNm << " " << funcLnNm << " " << bounds.toString());
    begLn = MIN(begLn, pScope->begLine());
    endLn = MAX(endLn, pScope->endLine());
    pScope->SetLineRange(begLn, endLn);
  }
  pScope->vmaSet().insert(bounds);
  
  return pScope;
}


//****************************************************************************

#if (DBG_PROC)
static bool testProcNow = false;
#endif

// BuildFromProc: Complete the representation for 'pScope' given the
// binutils::Proc 'p'.  Note that pScopes parent may itself be a ProcScope.
static ProcScope* 
BuildFromProc(ProcScope* pScope, binutils::Proc* p, bool irreducibleIntervalIsLoop)
{
  // FIXME: We can do better for lines
  // Look at the my parent or my sibling for a bound on line

#if (DBG_PROC)
  testProcNow = false;
  suint dbgId = p->GetId(); 

  const char* dbgNm = "main";
  if (strncmp(p->GetName(), dbgNm, strlen(dbgNm)) == 0) { testProcNow = true; }
  //if (dbgId == 537) { testProcNow = true; }

  //cout << "==> Processing `" << funcNm << "' (" << dbgId << ") <==\n";
#endif
  
  // -------------------------------------------------------
  // Build and traverse the Nested SCR (Tarjan tree) to create loop nests
  // -------------------------------------------------------
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
    if (testProcNow) {
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
    
    BuildFromTarjTree(pScope, p, tarj, cfg, fgRoot, irreducibleIntervalIsLoop);
    return pScope;
  }
  catch (const OA::Exception& x) {
    std::ostringstream os;
    x.report(os);
    DIAG_Throw("[OpenAnalysis] " << os.str());
  }
}


// BuildFromTarjTree: Recursively build loops using Nested SCR (Tarjan
// interval) analysis and returns the number of loops created.
static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, binutils::Proc* p,
		      OA::OA_ptr<OA::NestedSCR> tarj,
		      OA::OA_ptr<OA::CFG::Interface> cfg,
		      OA::RIFG::NodeId fgNode, CFGNodeToVMAMap* cfgNodeMap,
		      int addStmts, bool irreducibleIntervalIsLoop);

static int
BuildFromTarjTree(ProcScope* pScope, binutils::Proc* p, 
		  OA::OA_ptr<OA::NestedSCR> tarj,
		  OA::OA_ptr<OA::CFG::Interface> cfg, 
		  OA::RIFG::NodeId fgNode,
		  bool irreducibleIntervalIsLoop)
{
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  CFGNodeToVMAMap cfgNodeMap(cfg, p);
#else
  CFGNodeToVMAMap cfgNodeMap;
#endif
  int num = BuildFromTarjInterval(pScope, p, tarj, cfg, fgNode, 
				  &cfgNodeMap, 0, irreducibleIntervalIsLoop);
  return num;
}

static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, binutils::Proc* p,
		      OA::OA_ptr<OA::NestedSCR> tarj,
		      OA::OA_ptr<OA::CFG::Interface> cfg, 
		      OA::RIFG::NodeId fgNode, CFGNodeToVMAMap* cfgNodeMap,
		      int addStmts, bool irrIntIsLoop)
{
  int localLoops = 0;
  OA::OA_ptr<OA::RIFG> rifg = tarj->getRIFG();
  OA::OA_ptr<OA::CFG::Interface::Node> bb =
    rifg->getNode(fgNode).convert<OA::CFG::Interface::Node>();

#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  string func, file;
  VMA begVMA, endVMA;
  suint begLn = UNDEF_LINE, endLn = UNDEF_LINE;
  suint loopsBegLn = UNDEF_LINE, loopsEndLn = UNDEF_LINE;

  DIAG_Assert(cfgNodeMap->find(bb) != cfgNodeMap->end(), "");
  begVMA = (*cfgNodeMap)[bb]->first;
  endVMA = (*cfgNodeMap)[bb]->second;
#endif

  if (addStmts) {
    BuildFromBB(enclosingScope, p, bb);
  }
  
  // -------------------------------------------------------
  // Traverse the Nested SCR (Tarjan tree), building loop nests
  // -------------------------------------------------------
  for (int kid = tarj->getInners(fgNode); kid != OA::RIFG::NIL; 
       kid = tarj->getNext(kid) ) {
    OA::OA_ptr<OA::CFG::Interface::Node> bb1 = 
      rifg->getNode(kid).convert<OA::CFG::Interface::Node>();
    OA::NestedSCR::Node_t ity = tarj->getNodeType(kid);
    
    // -----------------------------------------------------
    // 1. ACYCLIC: No loops
    // -----------------------------------------------------
    if (ity == OA::NestedSCR::NODE_ACYCLIC) { 
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
      if (tarj->getNext(kid) == OA::RIFG::NIL) {
        DIAG_Assert(cfgNodeMap->find(bb1) != cfgNodeMap->end(), "");
        endVMA = (*cfgNodeMap)[bb1]->second;
      }
#endif
      if (addStmts) {
	BuildFromBB(enclosingScope, p, bb1);
      }
    }
    
    // -----------------------------------------------------
    // 2. INTERVAL or IRREDUCIBLE as a loop: Loop head
    // -----------------------------------------------------
    if (ity == OA::NestedSCR::NODE_INTERVAL || 
	(irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE)) { 
      
      suint begLn = FindLoopBegLine(p, bb1);

      // Build the loop nest
      LoopScope* lScope = new LoopScope(enclosingScope, begLn, begLn);
      int num = BuildFromTarjInterval(lScope, p, tarj, cfg, kid, 
				      cfgNodeMap, 1, irrIntIsLoop);
      localLoops += (num + 1);
      
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
      // Update line numbers from data collected building loop nest
      if (loopsBegLn == UNDEF_LINE) {
	DIAG_Assert(loopsEndLn == UNDEF_LINE, "");
	loopsBegLn = lScope->BegLine();
	loopsEndLn = lScope->EndLine();
      } 
      else {
	DIAG_Assert(loopsEndLn != UNDEF_LINE, "");
	if (IsValidLine(lScope->BegLine(), lScope->EndLine())) {
	  loopsBegLn = MIN(loopsBegLn, lScope->BegLine() );
	  loopsEndLn = MAX(loopsEndLn, lScope->EndLine() );
	}
      }
#endif
      
      // Remove the loop nest if we could not find valid line numbers
      if (!IsValidLine(lScope->begLine(), lScope->endLine())) {
	lScope->Unlink();
	delete lScope;
	localLoops -= (num + 1); // N.B.: 'num' should always be 0
      }
    }
    
    // -----------------------------------------------------
    // 3. IRREDUCIBLE as no loop: May contain loops
    // -----------------------------------------------------
    if (!irrIntIsLoop && ity == OA::NestedSCR::NODE_IRREDUCIBLE) {
      int num = BuildFromTarjInterval(enclosingScope, p, tarj, cfg, kid, 
				      cfgNodeMap, addStmts, irrIntIsLoop);
      localLoops += num;
    }
    
  }


#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  // FIXME: it would probably be better to include opIndices here
  p->GetSourceFileInfo(begVMA, 0, endVMA, 0, func, file, begLn, endLn);

  if (loopsBegLn != UNDEF_LINE && loopsBegLn < begLn) {
    begLn = loopsBegLn;
  }
  if (loopsEndLn != UNDEF_LINE && loopsEndLn > endLn) {
    endLn = loopsEndLn;
  }
  if (IsValidLine(begLn, endLn)) {
    enclosingScope->SetLineRange(begLn, endLn); // conditional
  }
#endif
  
  return localLoops;
}


// BuildFromBB: Adds statements from the current basic block to
// 'enclosingScope'.  Does not add duplicates.
static int 
BuildFromBB(CodeInfo* enclosingScope, binutils::Proc* p, 
	    OA::OA_ptr<OA::CFG::Interface::Node> bb)
{
  ProcScope* pScope = enclosingScope->Proc();
  FileScope* fileScope = pScope->File();

  // Use this map so we do not add multiple StmtRangeScopes for the
  // same line from this basic-block.  We could allow the
  // normalization phase to remove the redundancy, but this is an easy
  // optimization.
  std::map<suint, StmtRangeScope*> stmtMap;

  OA::OA_ptr<OA::CFG::Interface::NodeStatementsIterator> it =
    bb->getNodeStatementsIterator();
  for ( ; it->isValid(); ) {
    binutils::Insn* insn = IRHNDL_TO_TY(it->current(), binutils::Insn*);
    VMA vma = insn->GetVMA();
    ushort opIdx = insn->GetOpIndex();
    VMA opvma = binutils::LM::isa->ConvertVMAToOpVMA(vma, opIdx);

    // advance iterator [needed to create VMA interval]
    ++(*it);

    // -----------------------------------------------------
    // 1. find source line info (if possible)
    // -----------------------------------------------------
    string func, file;
    suint line;
    p->GetSourceFileInfo(vma, opIdx, func, file, line); 
    if ( !IsValidLine(line) ) {
      continue; // cannot continue without valid symbolic info
    }
    
    // -----------------------------------------------------
    // 2. Create a VMA interval for this line
    // -----------------------------------------------------
    VMA nextopvma;
    binutils::Insn* nextinsn = (it->isValid()) ? 
      IRHNDL_TO_TY(it->current(), binutils::Insn*) : NULL;
    if (nextinsn) {
      nextopvma = binutils::LM::isa->ConvertVMAToOpVMA(nextinsn->GetVMA(),
						     nextinsn->GetOpIndex());
    }
    else {
      // the (hypothetical) next insn must begins no earlier than:
      nextopvma = binutils::LM::isa->ConvertVMAToOpVMA(insn->GetVMA(), 0) 
	+ insn->GetSize();
    }
    DIAG_Assert(opvma < nextopvma, "Invalid VMAInterval: [" << opvma << ", "
		<< nextopvma << ")");

    VMAInterval vmaint(opvma, nextopvma);
    
    // -----------------------------------------------------
    // 3. Determine where this line should go
    // -----------------------------------------------------
    bool done = false;

    if (!file.empty() && file != fileScope->Name()) {
      // 3a. An alien line: Ignore for now (FIXME)
      done = true;
    }
    if (!done) {
      // 3b. Attempt to find a non-procedure enclosing scope, which
      // means LoopScope.  Note: We do not yet know loop end lines.
      // However, we can use the procedure end line as a boundary
      // because the only source code objects suceeding this scope
      // that can be multiply instantiated into this scope are
      // procedures. In contrast, Fortran statement functions or C
      // macros must preceed this scope.
      for (CodeInfo* x = enclosingScope;
	   x->Type() == ScopeInfo::LOOP; x = x->CodeInfoParent()) {
	if (x->begLine() <= line && line <= pScope->endLine()) {
	  // We found a home for the line
	  StmtRangeScope* stmt = stmtMap[line];
	  if (!stmt) {
	    stmtMap[line] = new StmtRangeScope(x, line, line, 
					       vmaint.beg(), vmaint.end());
	  }
	  else {
	    stmt->vmaSet().insert(vmaint); // expand VMA range
	  }
	  done = true;
	  break;
	}
      }
    }
    if (!done && pScope->begLine() <= line && line <= pScope->endLine()) {
      // 3c. An alien line that belongs within 'pScope'
      new StmtRangeScope(pScope, line, line, vmaint.beg(), vmaint.end());
      done = true;
    }
    if (!done) {
      // 3d. An alien line: ignore for now (FIXME)
      done = true;
    }
  } 
  return 0;
} 


// FindOrCreateFileNode: 
static FileScope* 
FindOrCreateFileNode(LoadModScope* lmScope, binutils::Proc* p)
{
  // Attempt to find filename for procedure
  string file = p->GetFilename();
  if (file.empty()) {
    string func;
    suint line;
    p->GetSourceFileInfo(p->GetBegVMA(), 0, func, file, line);
  }
  if (file.empty()) { 
    file = OrphanedProcedureFile; 
  }
  
  // Obtain corresponding FileScope
  FileScope* fileScope = lmScope->Pgm()->FindFile(file);
  if (fileScope == NULL) {
    bool fileIsReadable = FileIsReadable(file.c_str());
    fileScope = new FileScope(file, fileIsReadable, lmScope);
  }
  return fileScope; // guaranteed to be a valid pointer
} 


static suint 
FindLoopBegLine(binutils::Proc* p, OA::OA_ptr<OA::CFG::Interface::Node> node)
{
  // Given the head node of the loop, find the backward branch.

  // Note: It is possible to have multiple backward branches (e.g. an
  // 'unstructured' loop written with IFs and GOTOs).  We take the
  // smallest source line of them all.
  using namespace OA::CFG;

  suint begLn = UNDEF_LINE;

  // Find the head vma
  OA::OA_ptr<Interface::NodeStatementsIterator> stmtIt =
    node->getNodeStatementsIterator();
  DIAG_Assert(stmtIt->isValid(), "");
  binutils::Insn* head = IRHNDL_TO_TY(stmtIt->current(), binutils::Insn*);
  VMA headVMA = head->GetVMA(); // we can ignore opIdx
  
  // Now find the backward branch
  OA::OA_ptr<Interface::IncomingEdgesIterator> it 
    = node->getIncomingEdgesIterator();
  for ( ; it->isValid(); ++(*it)) {
    OA::OA_ptr<Interface::Edge> e = it->current();
    
    OA::OA_ptr<Interface::Node> bb = e->source();
    OA::OA_ptr<Interface::NodeStatementsRevIterator> stmtIt1 =
      bb->getNodeStatementsRevIterator();
    if (!stmtIt1->isValid()) {
      continue;
    }
    
    binutils::Insn* backbranch = IRHNDL_TO_TY(stmtIt1->current(), binutils::Insn*);
    VMA vma = backbranch->GetVMA();
    ushort opIdx = backbranch->GetOpIndex();

    // If we have a backward edge, find the source line of the
    // backward branch.  Note: back edges are not always labeled as such!
    if (e->getType() == Interface::BACK_EDGE || headVMA <= vma) {
      suint line;
      string func, file;
      p->GetSourceFileInfo(vma, opIdx, func, file, line); 
      if (IsValidLine(line) && (!IsValidLine(begLn) || line < begLn)) {
	begLn = line;
      }
    }
  }
  
  return begLn;
}

//****************************************************************************
// Helpers for normalizing a scope tree
//****************************************************************************

// RemoveOrphanedProcedureRepository: Remove the OrphanedProcedureFile
// from the tree.
static bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree)
{
  bool changed = false;

  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return changed; }

  for (ScopeInfoChildIterator lmit(pgmScope); lmit.Current(); lmit++) {
    DIAG_Assert(((ScopeInfo*)lmit.Current())->Type() == ScopeInfo::LM, "");
    LoadModScope* lm = dynamic_cast<LoadModScope*>(lmit.Current()); // always true

    // For each immediate child of this node...
    for (ScopeInfoChildIterator it(lm); it.Current(); /* */) {
      DIAG_Assert(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE, "");
      FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
      it++; // advance iterator -- it is pointing at 'file'
      
      if (file->Name() == OrphanedProcedureFile) {
        file->Unlink(); // unlink 'file' from tree
        delete file;
        changed = true;
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
//   multiple basic blocks containing the same statement, cf. BuildFromBB().
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
CoalesceDuplicateStmts(PgmScopeTree* pgmScopeTree, bool unsafeNormalizations)
{
  bool changed = false;
  CDS_unsafeNormalizations = unsafeNormalizations;
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  LineToStmtMap stmtMap;      // line to statement data map
  ScopeInfoSet visitedScopes; // all children of a scope have been visited
  ScopeInfoSet toDelete;      // nodes to delete

  for (ScopeInfoChildIterator lmit(pgmScope); lmit.Current(); lmit++) {
    DIAG_Assert(((ScopeInfo*)lmit.Current())->Type() == ScopeInfo::LM, "");
    LoadModScope* lm = dynamic_cast<LoadModScope*>(lmit.Current()); // always true
    
    // We apply the normalization routine to each FileScope so that 1)
    // we are guaranteed to only process CodeInfos and 2) we can assume
    // that all line numbers encountered are within the same file
    // (keeping the LineToStmtMap simple and fast).
    for (ScopeInfoChildIterator it(lm); it.Current(); ++it) {
      DIAG_Assert(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE, "");
      FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
      
      changed |= CoalesceDuplicateStmts(file, &stmtMap, &visitedScopes, 
					&toDelete, 1);
    } 
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
    else if (child->Type() == ScopeInfo::PROC) {
      stmtMap->clear();         // Clear statement table
      visited->clear();         // Clear visited set
      DeleteContents(toDelete); // Clear 'toDelete'
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
      for (ScopeInfoChildIterator it1(child); it1.Current(); /* */) {
	CodeInfo* i = dynamic_cast<CodeInfo*>(it1.Current()); // always true
	DIAG_Assert(i, "");
	it1++; // advance iterator -- it is pointing at 'i'
	
	i->Unlink();   // no longer a child of 'child'
	i->Link(node); // now a child of 'node'
      }
      child->Unlink(); // unlink 'child' from tree
      delete child;
      changed = true;
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
  if (node->Type() == ScopeInfo::FILE && node->ChildCount() == 0) {
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
  
  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); /* */) {
    DIAG_Assert(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE, "");
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    it++; // advance iterator -- it is pointing at 'file'

    // Verify this file in the current list of acceptible paths
    string baseFileName = BaseFileName(file->Name());
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
CFGNodeToVMAMap::build(OA::OA_ptr<OA::CFG::Interface> cfg, 
		      OA::OA_ptr<OA::CFG::Interface::Node> n, 
		      binutils::Proc* p, VMA _end)
{
  // NEWS FLASH: We could use OA's getReversePostDFSIterator to do this
  // pre-order traversal of the CFG.

  VMA curBeg = 0, curEnd = 0;

  // If n is not in the map, it hasn't been visited yet.
  if (this->find(n) == this->end()) {
    if (n->size() == 0) {
      // Empty blocks won't have any instructions to get VMAs from.
      if (n == cfg->getEntry()) {
        // Set the entry node VMAs from the procedure's begin address.
        curBeg = curEnd = p->GetBegVMA();
      } 
      else {
        // Otherwise, use the end VMA propagated from ancestors.
        curBeg = curEnd = _end;
      }
    } 
    else {
      // Non-empty blocks will have some instructions to get VMAs from.
      CFG_GetBegAndEndVMAs(n, curBeg, curEnd);
    }
    (*this)[n] = new std::pair<VMA, VMA>(curBeg, curEnd);

    // Visit descendents.
    OA::OA_ptr<OA::CFG::Interface::SinkNodesIterator> it = 
      n->getSinkNodesIterator();
    for ( ; it->isValid(); ++(*it)) {
      OA::OA_ptr<OA::CFG::Interface::Node> child = it->current();
      this->build(cfg, child, p, curEnd);
    }
  }
}

void
CFGNodeToVMAMap::clear()
{
  for (iterator it = this->begin(); it != this->end(); ++it) {
    delete (*it).second; // std::pair<VMA, VMA>*
  }
  My_t::clear();
}


static void 
CFG_GetBegAndEndVMAs(OA::OA_ptr<OA::CFG::Interface::Node> n, 
		     VMA& beg, VMA& end)
{
  beg = 0;
  end = 0;

  // FIXME: It would be a lot faster and easier if we could just grab
  // the first and last statements from a block. Perhaps we'll
  // modify the CFG code; this works for now.
  bool first = true;
  binutils::Insn* insn;
  OA::OA_ptr<OA::CFG::Interface::NodeStatementsIterator> it =
    n->getNodeStatementsIterator();
  for ( ; it->isValid(); ++(*it)) {
    if (first == true) {
      insn = IRHNDL_TO_TY(it->current(), binutils::Insn*);
      beg = insn->GetVMA();
      first = false;
    }
    insn = IRHNDL_TO_TY(it->current(), binutils::Insn*);
    end = insn->GetVMA();
  }
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
