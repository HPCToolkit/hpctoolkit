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
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
using std::cout;
using std::cerr;
using std::endl;

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <map> // STL

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/CFG/CFG.h>
#include <OpenAnalysis/CFG/OARIFG.h>
#include <OpenAnalysis/CFG/TarjanIntervals.h>

//*************************** User Include Files ****************************

#include "Args.h"
#include "PgmScopeTreeBuilder.h"
using namespace ScopeTreeBuilder;
#include "BloopIRInterface.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/Section.h>
#include <lib/binutils/Procedure.h>
#include <lib/binutils/BinUtils.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/support/Files.h>
#include <lib/support/Assertion.h>
#include <lib/support/pathfind.h>

//*************************** Forward Declarations ***************************

// Helpers for building a scope tree

static ProcScope*
BuildFromProc(FileScope* fileScope, Procedure* p, 
	      bool irreducibleIntervalIsLoop); 

static int
BuildFromTarjTree(ProcScope* pScope, Procedure* p, TarjanIntervals* tarj, 
		  RIFG* fg, CFG* cfg, RIFGNodeId fgNode,
		  bool irreducibleIntervalIsLoop);

static int
BuildFromBB(CodeInfo* enclosingScope, Procedure* p, CFG::Node* bb);

static FileScope*
FindOrCreateFileNode(LoadModScope* lmScope, Procedure* p);

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
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, String canonicalPathList);

//*************************** Forward Declarations ***************************

// ------------------------------------------------------------
// Helpers for traversing the Tarjan Tree
// ------------------------------------------------------------

// CFGNode -> <begAddr, endAddr>
class CFGNodeToPCMap : public std::map<CFG::Node*, std::pair<Addr, Addr>* > {
public:
  typedef std::map<CFG::Node*, std::pair<Addr, Addr>* > BaseMap;

public:
  CFGNodeToPCMap() { }
  CFGNodeToPCMap(CFG* cfg, Procedure* p) { build(cfg, cfg->Entry(), p, 0); }
  virtual ~CFGNodeToPCMap() { clear(); }
  virtual void clear();

  void build(CFG* cfg, CFG::Node* n, Procedure* p, Addr _end);
};

static void 
CFG_GetBegAndEndAddrs(CFG::Node* n, Addr &beg, Addr &end);


// ------------------------------------------------------------
// Helpers for normalizing the scope tree
// ------------------------------------------------------------
class StmtData;

class LineToStmtMap : public std::map<suint, StmtData*> {
public:
  typedef std::map<suint, StmtData*> BaseMap;

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


// MAP: Temporary and inadequate implementation.  Note more specific
// comments above function def.
static void 
BuildPCToSrcLineMap(PCToSrcLineXMap* map, Procedure* p); 

//*************************** Forward Declarations ***************************

//#define BLOOP_DEBUG_PROC
#define CDSDBG if (0) /* optimized away */

#define BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES

const char* OrphanedProcedureFile =
  "~~~No-Associated-File-Found-For-These-Procedures~~~";

const char *PGMdtd =
#include <lib/xml/PGM.dtd.h>

//****************************************************************************
// Set of routines to write a scope tree
//****************************************************************************

void
WriteScopeTree(std::ostream& os, PgmScopeTree* pgmScopeTree, bool prettyPrint)
{
  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE PGM [\n" << PGMdtd << "]>" << std::endl;
  os.flush();

  int dumpFlags = (PgmScopeTree::XML_TRUE); // ScopeInfo::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= PgmScopeTree::COMPRESSED_OUTPUT; }
  
  pgmScopeTree->Dump(os, dumpFlags);
}

//****************************************************************************
// Set of routines to build a scope tree
//****************************************************************************

// BuildFromExe: Builds a scope tree -- with a focus on loop nest
//   recovery -- representing program source code from the Executable
//   'exe'.  
// An executable represents binary code, which is essentially
//   a collection of procedures along with some extra information like
//   symbol and debugging tables.  This routine relies on the debugging
//   information to associate procedures with thier source code files
//   and to recover procedure loop nests, correlating them to source
//   code line numbers.  A normalization pass is typically run in order
//   to 'clean up' the resulting scope tree.  Among other things, the
//   normalizer employs heuristics to reverse certain compiler
//   optimizations such as loop unrolling.
PgmScopeTree*
ScopeTreeBuilder::BuildFromExe(/*Executable*/ LoadModule* exe, 
			       PCToSrcLineXMap* &map,
			       String canonicalPathList, 
			       bool normalizeScopeTree,
			       bool unsafeNormalizations,
			       bool irreducibleIntervalIsLoop,
			       bool verboseMode)
{
  BriefAssertion(exe);

  PgmScopeTree* pgmScopeTree = NULL;
  PgmScope* pgmScope = NULL;

  if (map != NULL) { // MAP
    map = new PCToSrcLineXMap();
  }

  // Assume exe->Read() has been performed
  pgmScope = new PgmScope();
  pgmScopeTree = new PgmScopeTree(pgmScope);

  LoadModScope *lmScope = new LoadModScope(exe->GetName(), pgmScope);
  
  // -----------------------------------------------------------------
  // For each procedure, find the existing corresponding FileScope or
  // create a new one; add procedure to FileScope and ScopeTree.
  // -----------------------------------------------------------------
  for (LoadModuleSectionIterator it(*exe); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) {
      continue;
    }
    
    // We have a TextSection.  Iterate over procedures.
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it1(*tsec); it1.IsValid(); ++it1) {
      Procedure* p = it1.Current();
      
      FileScope* fileScope = FindOrCreateFileNode(lmScope, p);
      if (verboseMode){
        cerr << "Building scope tree for [" << p->GetName()  << "] ... ";
      }
      BuildFromProc(fileScope, p, irreducibleIntervalIsLoop);
      if (map) { BuildPCToSrcLineMap(map, p); } // MAP
      if (verboseMode){
        cerr << "done " << endl;
      }
       
    }
  }

  if (map) { map->Finalize(); } // MAP
  
  if (canonicalPathList.Length() > 0) {
    bool result = FilterFilesFromScopeTree(pgmScopeTree, canonicalPathList);
    BriefAssertion(result); // Should never be false
  }

  if (normalizeScopeTree) {
    bool result = Normalize(pgmScopeTree, unsafeNormalizations);
    BriefAssertion(result); // Should never be false
  }

  return pgmScopeTree;
}


// Normalize: Because of compiler optimizations and other things, it
// is almost always desirable normalize a scope tree.  For example,
// almost all unnormalized scope tree contain duplicate statement
// instances.  See each normalizing routine for more information.
bool 
ScopeTreeBuilder::Normalize(PgmScopeTree* pgmScopeTree, 
			    bool unsafeNormalizations)
{
  bool changed = false;
  changed |= RemoveOrphanedProcedureRepository(pgmScopeTree);
  
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

#ifdef BLOOP_DEBUG_PROC
static bool testProcNow = false;
#endif

// BuildFromProc: 
static ProcScope* 
BuildFromProc(FileScope* fileScope, Procedure* p, 
	      bool irreducibleIntervalIsLoop)
{  
  String func, file;
  suint startLn, endLn;
  Instruction* eInst = p->GetLastInst();
  ushort endOp = (eInst) ? eInst->GetOpIndex() : 0;
  
  p->GetSourceFileInfo(p->GetStartAddr(), 0, p->GetEndAddr(), endOp,
		       func, file, startLn, endLn);

  String funcNm   = GetBestFuncName(p->GetName()); 
  String funcLnNm = GetBestFuncName(p->GetLinkName());

  ProcScope* pScope = new ProcScope((const char*)funcNm, fileScope,
				    (const char*)funcLnNm, startLn, endLn);

#ifdef BLOOP_DEBUG_PROC
  testProcNow = false;
  suint dbgId = p->GetId(); 

  const char* dbgNm = "main";
  if (strncmp(funcNm, dbgNm, strlen(dbgNm)) == 0) { testProcNow = true; }
  //if (dbgId == 537) { testProcNow = true; }

  //cout << "==> Processing `" << funcNm << "' (" << dbgId << ") <==\n";
#endif
  
  // -------------------------------------------------------
  // Build and traverse the Tarjan tree to create loop nests
  // -------------------------------------------------------
  BloopIRInterface irInterface(p);
  BloopIRStmtIterator stmtIter(*p);
  
  CFG cfg(irInterface, &stmtIter, 
	  PTR_TO_IRHNDL((const char *)funcNm, SymHandle));
  OARIFG fg(cfg);
  TarjanIntervals tarj(fg);
  RIFGNodeId fgRoot = fg.GetRootNode();
  
#ifdef BLOOP_DEBUG_PROC
  if (testProcNow) {
    cout << "*** CFG for `" << funcNm << "' ***" << endl;
    cout << "  total blocks: " << cfg.num_nodes() << endl
	 << "  total edges:  " << cfg.num_edges() << endl;
    cfg.dump(cout);
    cfg.dumpdot(cout);

    cout << "*** Tarjan Interval Tree for `" << funcNm << "' ***" << endl;
    tarj.Dump();
    cout << endl;
    cout.flush(); cerr.flush();
  }
#endif 
  
  BuildFromTarjTree(pScope, p, &tarj, &fg, &cfg, fgRoot, 
		    irreducibleIntervalIsLoop);
  return pScope;
}


// BuildFromTarjTree: Recursively build loops using Tarjan interval
// analysis and returns the number of loops created.
static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, Procedure* p,
		      TarjanIntervals* tarj, RIFG* fg, CFG* cfg, 
		      RIFGNodeId fgNode, CFGNodeToPCMap* cfgNodeMap,
		      int addStmts, bool irreducibleIntervalIsLoop);

static int
BuildFromTarjTree(ProcScope* pScope, Procedure* p, TarjanIntervals* tarj, 
		  RIFG* fg, CFG* cfg, RIFGNodeId fgNode,
		  bool irreducibleIntervalIsLoop)
{
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  CFGNodeToPCMap cfgNodeMap(cfg, p);
#else
  CFGNodeToPCMap cfgNodeMap;
#endif
  int num = BuildFromTarjInterval(pScope, p, tarj, fg, cfg, fgNode, 
				  &cfgNodeMap, 0, irreducibleIntervalIsLoop);
  return num;
}

static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, Procedure* p,
		      TarjanIntervals* tarj, RIFG* fg, CFG* cfg, 
		      RIFGNodeId fgNode, CFGNodeToPCMap* cfgNodeMap,
		      int addStmts, bool irrIntIsLoop)
{
  int localLoops = 0;
  CFG::Node* bb = (CFG::Node*)(fg->GetRIFGNode(fgNode));

#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  String func, file;
  Addr startAddr, endAddr;
  suint startLn = UNDEF_LINE, endLn = UNDEF_LINE;
  suint loopsStartLn = UNDEF_LINE, loopsEndLn = UNDEF_LINE;

  BriefAssertion(cfgNodeMap->find(bb) != cfgNodeMap->end());
  startAddr = (*cfgNodeMap)[bb]->first;
  endAddr = (*cfgNodeMap)[bb]->second;
#endif

  if (addStmts) {
    BuildFromBB(enclosingScope, p, bb);
  }
  
  // -------------------------------------------------------
  // Traverse the Tarjan tree, building loop nests
  // -------------------------------------------------------
  for (int kid = tarj->TarjInners(fgNode); kid != RIFG_NIL; 
       kid = tarj->TarjNext(kid) ) {
    CFG::Node* crtBlk = (CFG::Node*)(fg->GetRIFGNode(kid));

    // -----------------------------------------------------
    // 1. ACYCLIC: No loops
    // -----------------------------------------------------
    if (tarj->IntervalType(kid) == RI_TARJ_ACYCLIC) { 
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
      if (tarj->TarjNext(kid) == RIFG_NIL) {
        BriefAssertion(cfgNodeMap->find(crtBlk) != cfgNodeMap->end());
        endAddr = (*cfgNodeMap)[crtBlk]->second;
      }
#endif
      if (addStmts) {
	BuildFromBB(enclosingScope, p, crtBlk);
      }
    }
    
    // -----------------------------------------------------
    // 2. INTERVAL or IRREDUCIBLE as a loop: Loop head
    // -----------------------------------------------------
    if (tarj->IntervalType(kid) == RI_TARJ_INTERVAL || 
	(irrIntIsLoop && tarj->IntervalType(kid) == RI_TARJ_IRREDUCIBLE)) { 
      
      // Build the loop nest
      LoopScope* lScope = new LoopScope(enclosingScope, UNDEF_LINE,
					UNDEF_LINE, crtBlk->getId());
      int num = BuildFromTarjInterval(lScope, p, tarj, fg, cfg, kid, 
				      cfgNodeMap, 1, irrIntIsLoop);
      localLoops += (num + 1);
      
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
      // Update line numbers from data collected building loop nest
      if (loopsStartLn == UNDEF_LINE) {
	BriefAssertion( loopsEndLn == UNDEF_LINE );
	loopsStartLn = lScope->BegLine();
	loopsEndLn = lScope->EndLine();
      } 
      else {
	BriefAssertion( loopsEndLn != UNDEF_LINE );
	if (IsValidLine(lScope->BegLine(), lScope->EndLine())) {
	  loopsStartLn = MIN(loopsStartLn, lScope->BegLine() );
	  loopsEndLn = MAX(loopsEndLn, lScope->EndLine() );
	}
      }
#endif
      
      // Remove the loop nest if we could not find valid line numbers
      if (!IsValidLine(lScope->BegLine(), lScope->EndLine())) {
	lScope->Unlink();
	delete lScope;
	localLoops -= (num + 1); // N.B.: 'num' should always be 0
      }
    }
    
    // -----------------------------------------------------
    // 3. IRREDUCIBLE as no loop: May contain loops
    // -----------------------------------------------------
    if (!irrIntIsLoop && tarj->IntervalType(kid) == RI_TARJ_IRREDUCIBLE) {
      int num = BuildFromTarjInterval(enclosingScope, p, tarj, fg, cfg, 
				      kid, cfgNodeMap, addStmts, irrIntIsLoop);
      localLoops += num;
    }
    
  }


#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  // FIXME: it would probably be better to include opIndices here
  p->GetSourceFileInfo(startAddr, 0, endAddr, 0, func, file, startLn, endLn);

  if (loopsStartLn != UNDEF_LINE && loopsStartLn < startLn) {
    startLn = loopsStartLn;
  }
  if (loopsEndLn != UNDEF_LINE && loopsEndLn > endLn) {
    endLn = loopsEndLn;
  }
  if (IsValidLine(startLn, endLn)) {
    enclosingScope->SetLineRange(startLn, endLn); // conditional
  }
#endif
  
  return localLoops;
}


// BuildFromBB: Adds statements from the current basic block to
// 'enclosingScope'.  Does not add duplicates.
static int 
BuildFromBB(CodeInfo* enclosingScope, Procedure* p, CFG::Node* bb)
{
  LineToStmtMap stmtMap; // maps lines to NULL (simulates a set)

  for (CFG::NodeStatementsIterator s_iter(bb); (bool)s_iter; ++s_iter) {
    Instruction* insn = IRHNDL_TO_PTR((StmtHandle)s_iter, Instruction*);
    Addr pc = insn->GetPC();
    ushort opIdx = insn->GetOpIndex();

    String func, file;
    suint line;
    p->GetSourceFileInfo(pc, opIdx, func, file, line); 
    if ( !IsValidLine(line) ) {
      continue; // cannot continue without valid symbolic info
    }
    
    // eraxxon: MAP: add all PC's for BB to map
    
    if (stmtMap.find(line) == stmtMap.end()) {
      stmtMap[line] = NULL;
      new StmtRangeScope(enclosingScope, line, line);
    }
  } 
  return 0;
} 


// FindOrCreateFileNode: 
static FileScope* 
FindOrCreateFileNode(LoadModScope* lmScope, Procedure* p)
{
  String func, file;
  suint startLn, endLn;
  p->GetSourceFileInfo(p->GetStartAddr(), 0, p->GetEndAddr(), 0, func, file,
		       startLn, endLn); // FIXME: use only startAddr

  if (file.Empty()) { file = OrphanedProcedureFile; }
  FileScope* fileScope = lmScope->FindFile(file);
  if (fileScope == NULL) {
    bool fileIsReadable = FileIsReadable(file);
    fileScope = new FileScope(file, fileIsReadable, lmScope);
  }
  return fileScope; // guaranteed to be a valid pointer
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
    BriefAssertion(((ScopeInfo*)lmit.Current())->Type() == ScopeInfo::LM);
    LoadModScope* lm = dynamic_cast<LoadModScope*>(lmit.Current()); // always true

    // For each immediate child of this node...
    for (ScopeInfoChildIterator it(lm); it.Current(); /* */) {
      BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
      FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
      it++; // advance iterator -- it is pointing at 'file'
      
      if (strcmp(file->Name(), OrphanedProcedureFile) == 0) {
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
		       int level)
  throw (CDS_RestartException);

static bool
CDS_Main(CodeInfo* scope, LineToStmtMap* stmtMap, 
	 ScopeInfoSet* visited, ScopeInfoSet* toDelete, int level)
  throw (CDS_RestartException); /* indirect */

static bool
CDS_InspectStmt(StmtRangeScope* stmt1, LineToStmtMap* stmtMap, 
		ScopeInfoSet* toDelete, int level)
  throw (CDS_RestartException);

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
    BriefAssertion(((ScopeInfo*)lmit.Current())->Type() == ScopeInfo::LM);
    LoadModScope* lm = dynamic_cast<LoadModScope*>(lmit.Current()); // always true
    
    // We apply the normalization routine to each FileScope so that 1)
    // we are guaranteed to only process CodeInfos and 2) we can assume
    // that all line numbers encountered are within the same file
    // (keeping the LineToStmtMap simple and fast).
    for (ScopeInfoChildIterator it(lm); it.Current(); ++it) {
      BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
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
  throw (CDS_RestartException)
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
  throw (CDS_RestartException) /* indirect */
{
  bool changed = false;
  
  if (!scope) { return changed; }
  if (visited->find(scope) != visited->end()) { return changed; }
  if (toDelete->find(scope) != toDelete->end()) { return changed; }

  // A post-order traversal of this node (visit children before parent)...
  for (ScopeInfoChildIterator it(scope); it.Current(); ++it) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    BriefAssertion(child);
    
    if (toDelete->find(child) != toDelete->end()) { continue; }
    
    CDSDBG { cout << "CDS: " << child << endl; }
    
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
  throw (CDS_RestartException)
{
  bool changed = false;
  
  suint line = stmt1->BegLine();
  StmtData* stmtdata = (*stmtMap)[line];
  if (stmtdata) {
    
    StmtRangeScope* stmt2 = stmtdata->GetStmt();
    CDSDBG { cout << " Find: " << stmt1 << " " << stmt2 << endl; }
    
    // Ensure we have two different instances of the same line
    if (stmt1 == stmt2) { return false; }
    
    // Find the least common ancestor.  At most it should be a
    // procedure scope.
    ScopeInfo* lca = ScopeInfo::LeastCommonAncestor(stmt1, stmt2);
    BriefAssertion(lca);
    
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
      CDSDBG { cout << "  Delete: " << toRemove << endl; }
      changed = true;
    } 
    else if (CDS_unsafeNormalizations) {
      // Case 2: Duplicate statements in different loops (or scopes).
      // Merge the nodes from stmt2->lca into those from stmt1->lca.
      CDSDBG { cout << "  Merge: " << stmt1 << " <- " << stmt2 << endl; }
      changed = ScopeInfo::MergePaths(lca, stmt1, stmt2);
      if (changed) {
	// We may have created instances of case 1.  Furthermore,
	// while neither statement is deleted ('stmtMap' is still
	// valid), iterators between stmt1 and 'lca' are invalidated.
	// Restart at lca.
	CodeInfo* lca_CI = dynamic_cast<CodeInfo*>(lca);
	BriefAssertion(lca_CI);
	throw CDS_RestartException(lca_CI);
      }
    }
  } 
  else {
    // Add the statement instance to the map
    stmtdata = new StmtData(stmt1, level);
    (*stmtMap)[line] = stmtdata;
    CDSDBG { cout << " Map: " << stmt1 << endl; }
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
    BriefAssertion(child);
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
    if (perfNested && IsValidLine(child->BegLine(), child->EndLine()) &&
	child->BegLine() == n_CI->BegLine() &&
	child->EndLine() == n_CI->EndLine()) { 
      
      // Move all children of 'child' so that they are children of 'node'
      for (ScopeInfoChildIterator it1(child); it1.Current(); /* */) {
	CodeInfo* i = dynamic_cast<CodeInfo*>(it1.Current()); // always true
	BriefAssertion(i);
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

// RemoveEmptyScopes: Removes certain empty scopes from the tree,
// always maintaining the top level PgmScope (PGM) scope.  The
// following scope are deleted if empty:
//   FileScope, ProcScope, LoopScope
static bool 
RemoveEmptyScopes(ScopeInfo* node);

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
    if (child->Type() == ScopeInfo::FILE 
	|| child->Type() == ScopeInfo::PROC
	|| child->Type() == ScopeInfo::LOOP) {
      if (child->ChildCount() == 0) {
	child->Unlink(); // unlink 'child' from tree
	delete child;
	changed = true;
      }
    }
  } 
  
  return changed; 
}

//****************************************************************************

// FilterFilesFromScopeTree: 
static bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, String canonicalPathList)
{
  bool changed = false;
  
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return changed; }
  
  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); /* */) {
    BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    it++; // advance iterator -- it is pointing at 'file'

    // Verify this file in the current list of acceptible paths
    String baseFileName = BaseFileName(file->Name());
    BriefAssertion(baseFileName.Length() > 0);
    if (!pathfind(canonicalPathList, baseFileName, "r")) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
      changed = true;
    }
  } 
  
  return changed;
}


//****************************************************************************
// Helpers for traversing the Tarjan Tree
//****************************************************************************

void
CFGNodeToPCMap::build(CFG* cfg, CFG::Node* n, Procedure* p, Addr _end)
{
  Addr curBeg = 0, curEnd = 0;

  // If n is not in the map, it hasn't been visited yet.
  if (this->find(n) == this->end()) {
    if (n->empty() == true) {
      // Empty blocks won't have any instructions to get PCs from.
      if (n == cfg->Entry()) {
        // Set the entry node PCs from the procedure's begin address.
        curBeg = curEnd = p->GetStartAddr();
      } 
      else {
        // Otherwise, use the end PC propagated from ancestors.
        curBeg = curEnd = _end;
      }
    } 
    else {
      // Non-empty blocks will have some instructions to get PCs from.
      CFG_GetBegAndEndAddrs(n, curBeg, curEnd);
    }
    (*this)[n] = new std::pair<Addr, Addr>(curBeg, curEnd);

    // Visit descendents.
    for (CFG::SinkNodesIterator sink(n); (bool)sink; ++sink) {
      CFG::Node* child = dynamic_cast<CFG::Node*>((DGraph::Node*)sink);
      this->build(cfg, child, p, curEnd);
    }
  }
}

void
CFGNodeToPCMap::clear()
{
  for (iterator it = this->begin(); it != this->end(); ++it) {
    delete (*it).second; // std::pair<Addr, Addr>*
  }
  BaseMap::clear();
}


static void 
CFG_GetBegAndEndAddrs(CFG::Node* n, Addr &beg, Addr &end)
{
  beg = 0;
  end = 0;

  // FIXME: It would be a lot faster and easier if we could just grab
  // the first and last statements from a block. Perhaps we'll
  // modify the CFG code, this works for now.
  bool first = true;
  Instruction* insn;
  for (CFG::NodeStatementsIterator s_iter(n); (bool)s_iter; ++s_iter) {
    if (first == true) {
      insn = IRHNDL_TO_PTR((StmtHandle)s_iter, Instruction*);
      beg = insn->GetPC();
      first = false;
    }
    insn = IRHNDL_TO_PTR((StmtHandle)s_iter, Instruction*);
    end = insn->GetPC();
  }
}

//****************************************************************************
// Helpers for traversing the Tarjan Tree
//****************************************************************************

void
LineToStmtMap::clear()
{
  for (BaseMap::iterator it = begin(); it != end(); ++it) {
    delete (*it).second;
  }
  BaseMap::clear();
}

//****************************************************************************
// Helper Routines
//****************************************************************************

// MAP: This is a temporary and inadequate implementation for building
// the [PC -> src-line] map.  In particular, the map should be built
// with the source-code recovery information that is computed as the
// ScopeTree is construced, esp. the loop nesting information.  (For
// example, perhaps the map should be constructed with the ScopeTree).

static void 
BuildPCToSrcLineMap(PCToSrcLineXMap* map, Procedure* p)
{
  //suint dbgId = p->GetId(); 
  String theFunc = GetBestFuncName(p->GetName()); 
  String theFile = "";
  
  ProcPCToSrcLineXMap* pmap = 
    new ProcPCToSrcLineXMap(p->GetStartAddr(), p->GetEndAddr(), 
			    theFunc, theFile);
  
  for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
    Instruction* inst = it.Current();
    Addr pc = inst->GetPC();
    
    // 1. Attempt to find symbolic information
    String func, file;
    suint line;
    p->GetSourceFileInfo(pc, inst->GetOpIndex(), func, file, line);
    
    if ( !IsValidLine(line) ) { continue; } // need valid symbolic info
    if (theFile.Empty() && !file.Empty()) {
      theFile = file; 
    }
    
#if 0
    cerr << hex << pc << dec << ": " 
	 << file << ":" << func << ":" << line << endl;
#endif
    
    // 2. Update 'pmap'
    SrcLineX* lineInstance = new SrcLineX(line, 0); // MAP
    pmap->Insert(pc, lineInstance);
  }
  
  pmap->SetFileName(theFile);
  map->InsertProcInList(pmap);
}


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
