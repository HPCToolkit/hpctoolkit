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
//    PgmScopeTreeBuilder.C
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
#include "PgmScopeTreeUtils.h"
using namespace ScopeTreeBuilder;
#include "CodeInfoPtrSet.h"
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
BuildFromProc(FileScope* fileScope, Procedure* p); 

static int
BuildFromTarjTree(ProcScope* pScope, Procedure* p, TarjanIntervals* tarj, 
		  RIFG* fg, CFG* cfg, RIFGNodeId fgNode);

static int
BuildFromBB(CodeInfo* enclosingScope, Procedure* p, CFG::Node* bb);

static FileScope*
FindOrCreateFileNode(PgmScope* pgmScopeTree, Procedure* p);

//*************************** Forward Declarations ***************************

// Helpers for normalizing a scope tree

static bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree);

static bool 
MergeOverlappingCode(PgmScopeTree* pgmScopeTree);

static bool
RenestUnnestedStmts(PgmScopeTree* pgmScopeTree);

static bool 
MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree);

static bool 
RemoveEmptyScopes(PgmScopeTree* pgmScopeTree);

static bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, String canonicalPathList);

//*************************** Forward Declarations ***************************

// Helpers for traversing the Tarjan Tree
// CFGNode -> <begAddr, endAddr>
class CFGNodeToPCMap : public std::map<CFG::Node*, std::pair<Addr, Addr>* > {
public:
  CFGNodeToPCMap();
  CFGNodeToPCMap(CFG* cfg, Procedure* p);
  virtual ~CFGNodeToPCMap();
  virtual void clear();

  void build(CFG* cfg, CFG::Node* n, Procedure* p, Addr _end);

public:
  typedef std::map<CFG::Node*, std::pair<Addr, Addr>* > BaseMap;
};

static void 
CFG_GetBegAndEndAddrs(CFG::Node* n, Addr &beg, Addr &end);


// Helpers for normalizing the scope tree
class StmtData;

typedef std::map<suint, StmtData*> LineToStmtMap;
typedef std::map<suint, StmtData*>::iterator LineToStmtMapIt;

class StmtData {
public:
  StmtData(StmtRangeScope* _stmt = NULL, int _level = 0)
    : stmt(_stmt), level(_level) { }
  ~StmtData() { /* owns no data */ }

  StmtRangeScope* GetStmt() { return stmt; }
  int GetLevel() { return level; }
  
  void SetStmt(StmtRangeScope* _stmt) { stmt = _stmt; }
  void SetLevel(int _level) { level = _level; }
  
private:
  StmtRangeScope* stmt;
  int level;
};


// MAP: Temporary and inadequate implementation.  Note more specific
// comments above function def.
static void 
BuildPCToSrcLineMap(PCToSrcLineXMap* map, Procedure* p); 

//*************************** Forward Declarations ***************************

//#define BLOOP_DEBUG_PROC
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

PgmScopeTree*
ScopeTreeBuilder::BuildFromExe(Executable* exe, PCToSrcLineXMap* &map,
			       String canonicalPathList, 
			       bool normalizeScopeTree,
			       bool verboseMode)
{
  BriefAssertion(exe);

  PgmScopeTree* pgmScopeTree = NULL;
  PgmScope* pgmScope = NULL;

  if (map != NULL) { // MAP
    map = new PCToSrcLineXMap();
  }

  // Assume exe->Read() has been performed
  pgmScope = new PgmScope(exe->GetName());
  pgmScopeTree = new PgmScopeTree(pgmScope);
  
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
      
      FileScope* fileScope = FindOrCreateFileNode(pgmScope, p);
      if (verboseMode){
        cerr << "Building scope tree for [" << p->GetName()  << "] ... ";
      }
      BuildFromProc(fileScope, p);
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
    bool result = Normalize(pgmScopeTree);
    BriefAssertion(result); // Should never be false
  }

  return pgmScopeTree;
}

bool 
ScopeTreeBuilder::Normalize(PgmScopeTree* pgmScopeTree)
{
  RemoveOrphanedProcedureRepository(pgmScopeTree);
  
  // Notes: The order of the passes is important.  Also, thus far, we should
  // not actually need the iteration to achieve a fixed point.
  bool changed = false;
  do {
    bool ch1, ch2, ch3;
    ch1 = MergeOverlappingCode(pgmScopeTree);
    ch2 = RenestUnnestedStmts(pgmScopeTree);
    ch3 = MergePerfectlyNestedLoops(pgmScopeTree);
    changed = (ch1 || ch2 || ch3);
  } while (changed);
  
  changed |= RemoveEmptyScopes(pgmScopeTree);
  
  return true; // no errors
}

//****************************************************************************
// Helpers for building a scope tree
//****************************************************************************

#ifdef BLOOP_DEBUG_PROC
static bool testProcNow = false;
#endif

static ProcScope* 
BuildFromProc(FileScope* fileScope, Procedure* p)
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
  
  CFG cfg(irInterface, &stmtIter, (SymHandle)((const char *)funcNm));
  OARIFG fg(cfg);
  TarjanIntervals tarj(fg);
  RIFGNodeId fgRoot = fg.GetRootNode();
  
#ifdef BLOOP_DEBUG_PROC
  if (testProcNow) {
    cout << "*** CFG for `" << funcNm << "' ***" << endl;
    cout << "  total blocks: " << cfg.num_nodes() << endl
	 << "  total edges:  " << cfg.num_edges() << endl;
    cfg.dump(cout);

    cout << "*** Tarjan Interval Tree for `" << funcNm << "' ***" << endl;
    tarj.Dump();
    cout << endl;
    cout.flush(); cerr.flush();
  }
#endif 
  
  BuildFromTarjTree(pScope, p, &tarj, &fg, &cfg, fgRoot);
  return pScope;
}


// BuildFromTarjTree: Recursively build loops using Tarjan interval
// analysis and returns the number of loops created.
static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, Procedure* p,
		      TarjanIntervals* tarj, RIFG* fg, CFG* cfg, 
		      RIFGNodeId fgNode, CFGNodeToPCMap* cfgNodeMap,
		      int addStmts);

static int
BuildFromTarjTree(ProcScope* pScope, Procedure* p, TarjanIntervals* tarj, 
		  RIFG* fg, CFG* cfg, RIFGNodeId fgNode)
{
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
  CFGNodeToPCMap cfgNodeMap(cfg, p);
#else
  CFGNodeToPCMap cfgNodeMap;
#endif
  int num = BuildFromTarjInterval(pScope, p, tarj, fg, cfg, fgNode, 
				  &cfgNodeMap, 0);
  return num;
}

static int 
BuildFromTarjInterval(CodeInfo* enclosingScope, Procedure* p,
		      TarjanIntervals* tarj, RIFG* fg, CFG* cfg, 
		      RIFGNodeId fgNode, CFGNodeToPCMap* cfgNodeMap,
		      int addStmts)
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
    // 2. INTERVAL: Loop head
    // -----------------------------------------------------
    if (tarj->IntervalType(kid) == RI_TARJ_INTERVAL) { 
      
      // Build the loop nest
      LoopScope* lScope = new LoopScope(enclosingScope, UNDEF_LINE,
					UNDEF_LINE, crtBlk->getID());
      int num = BuildFromTarjInterval(lScope, p, tarj, fg, cfg, kid, 
				      cfgNodeMap, 1);
      localLoops += (num + 1);
      
#ifdef BLOOP_ATTEMPT_TO_IMPROVE_INTERVAL_BOUNDARIES
      // Update line numbers from data collected building loop nest
      if (loopsStartLn == UNDEF_LINE) {
	BriefAssertion( loopsEndLn == UNDEF_LINE );
	loopsStartLn = lScope->BegLine();
	loopsEndLn = lScope->EndLine();
      } else {
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
    // 3. IRREDUCIBLE: May contain loops
    // -----------------------------------------------------
    if (tarj->IntervalType(kid) == RI_TARJ_IRREDUCIBLE) {
      int num = BuildFromTarjInterval(enclosingScope, p, tarj, fg, cfg, 
				      kid, cfgNodeMap, addStmts);
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
  LineToStmtMap stmtMap; // maps lines to the bogus pointer (StmtData*)1

  for (CFG::NodeStatementsIterator s_iter(bb); (bool)s_iter; ++s_iter) {
    Instruction *insn = (Instruction *)((StmtHandle)s_iter);
    Addr pc = insn->GetPC();
    
    String func, file;
    suint line;
    p->GetSourceFileInfo(pc, insn->GetOpIndex(), func, file, line); 
    if ( !IsValidLine(line) ) {
      continue; // cannot continue without valid symbolic info
    }
    
    // eraxxon: MAP: add all PC's for BB to map
    
    StmtData* stmtdata = stmtMap[line];
    if (stmtdata == NULL) {
      stmtMap[line] = (StmtData*)1;
      new StmtRangeScope(enclosingScope, line, line);
    }
  } 
  return 0;
} 


// FindOrCreateFileNode: 
static FileScope* 
FindOrCreateFileNode(PgmScope* pgmScopeTree, Procedure* p)
{
  String func, file;
  suint startLn, endLn;
  p->GetSourceFileInfo(p->GetStartAddr(), 0, p->GetEndAddr(), 0, func, file,
		       startLn, endLn); // FIXME: use only startAddr

  if (file.Empty()) { file = OrphanedProcedureFile; }
  FileScope* fileScope = pgmScopeTree->FindFile(file);
  if (fileScope == NULL) {
    bool fileIsReadable = FileIsReadable(file);
    fileScope = new FileScope(file, fileIsReadable, pgmScopeTree);
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

  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); /* */) {
    BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    it++; // advance iterator -- it is pointing at 'file'
    
    if (strcmp(file->Name(), OrphanedProcedureFile) == 0) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
      changed = true;
    }
  } 
  
  return changed;
}


// MergeOverlappingCode: When any particular loop or statement
// overlaps with another [at the same level, having the same parent
// loop], fuse them.  
//
// Rationale: Compiler optimizations such as loop unrolling (start-up,
// steady-state, wind-down), e.g., can produce (strange!) cases of
// duplicate and overlapping code.
static bool 
MergeOverlappingCode(ScopeInfo* node);

static bool 
MergeOverlappingCode(PgmScopeTree* pgmScopeTree)
{
  return MergeOverlappingCode(pgmScopeTree->GetRoot());
}

static bool 
MergeOverlappingCode(ScopeInfo* node)
{
  bool changed = false;
  
  if (!node) { return changed; }
  
  // Create a set with all the children of newItem and iterate over this in
  // order not to be affected by a modification in the tree structure
  CodeInfoPtrSet* childSet = MakeSetFromChildren(node);
  
  // For each immediate child of this node...
  CodeInfoPtrSet* mergedSet = new CodeInfoPtrSet;
  CodeInfo *child = NULL;
  for (CodeInfoPtrSetIterator iter(childSet); (child = iter.Current());
       iter++) {
    BriefAssertion(child); // always true

    // 1. Recursively do any merging for this tree's children
    changed |= MergeOverlappingCode(child);

    // 2. Test for overlapping loops and merge if necessary
    if (child->Type() == ScopeInfo::LOOP) {
      
      // Collect loops in a set that prevents overlapping elements;
      // AddOrMerge may unlink 'child' from tree and delete it.
      bool added = mergedSet->AddOrMerge(child); 
      changed = !added; // if not added, then it was merged
    }
  } 
  delete mergedSet;
  delete childSet;
  
  return changed; 
}


// RenestUnnestedStmts: If the same statement exists at multiple
// levels within a loop nest, discard all but the innermost
// instance. If two identical statements exist at the same level,
// discard one.  (Multiple statements exist at the same level because
// of multiple basic blocks containing the same statement.)
//
// Rationale: Compiler optimizations may hoist loop-invariant
// operations out of inner loops.  Note however that in rare cases,
// code may be moved deeper into a nest (e.g. to free registers).
// Even so, we want to associate statements within loops nests to
// the deepest level in which they appear.
static bool
RenestUnnestedStmts(CodeInfo *scope, LineToStmtMap* stmtMap, int level);

static bool
RenestUnnestedStmts(PgmScopeTree* pgmScopeTree)
{
  bool changed = false;
  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  
  // Note: we apply the normalization routine to each FileScope so
  // that it can assume that all line numbers encountered are within
  // the same file.  This keeps the LineToStmtMap simple and fast.

  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); ++it) {
    BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    
    LineToStmtMap stmtMap;
    changed |= RenestUnnestedStmts(file, &stmtMap, 0);
  } 
  
  return changed;
}

// RenestUnnestedStmts: Helper for the above. Assumes that all
// statement line numbers are within the same file.  Note that since
// we may delete nodes, we always operate on the children of 'scope'.
// (Deleting oneself is not fun!)
static bool
RenestUnnestedStmts(CodeInfo *scope, LineToStmtMap* stmtMap, int level)
{
  bool changed = false;
  
  if (!scope) { return changed; }
  
  // A post-order traversal of this node (visit children before parent)...
  for (ScopeInfoChildIterator it(scope); it.Current(); /* */) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    BriefAssertion(child);
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively perform re-nesting on 'child'
    changed |= RenestUnnestedStmts(child, stmtMap, level + 1);
    
    // 2. Examine 'child'
    if (child->Type() == ScopeInfo::STMT_RANGE) {
      
      // If this statement instance already exists, delete the
      // shallower instance.
      StmtRangeScope* sr_child = dynamic_cast<StmtRangeScope*>(child);
      suint line = child->BegLine();
      StmtData* stmtdata = (*stmtMap)[line];
      if (stmtdata) {
	
	// Find the node stmt to delete
	StmtRangeScope* toRemove = NULL;
	if (stmtdata->GetLevel() < level) {
	  toRemove = stmtdata->GetStmt();
	  stmtdata->SetStmt(sr_child);  // set replacement info
	  stmtdata->SetLevel(level);
	} else { 
	  toRemove = sr_child;
	}
	toRemove->Unlink(); // unlink 'toRemove' from tree
	delete toRemove;
	changed = true;
	
      } else {
	stmtdata = new StmtData(sr_child, level);
	(*stmtMap)[line] = stmtdata;
      }
      
    } else if (child->Type() == ScopeInfo::PROC) {
      // Clear statement table
      for (LineToStmtMapIt it = stmtMap->begin(); it != stmtMap->end(); ++it) {
	delete (*it).second;
      }
      stmtMap->clear();
    }
  } 
  
  return changed; 
}


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

CFGNodeToPCMap::CFGNodeToPCMap() 
{
}

CFGNodeToPCMap::CFGNodeToPCMap(CFG* cfg, Procedure* p)
{
  this->build(cfg, cfg->Entry(), p, 0);
}

CFGNodeToPCMap::~CFGNodeToPCMap()
{
  clear();
}

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
      } else {
        // Otherwise, use the end PC propagated from ancestors.
        curBeg = curEnd = _end;
      }
    } else {
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
  for (CFG::NodeStatementsIterator s_iter(n); (bool)s_iter; ++s_iter) {
    if (first == true) {
      beg = ((Instruction *)((StmtHandle)s_iter))->GetPC();
      first = false;
    }
    end = ((Instruction *)((StmtHandle)s_iter))->GetPC();
  }
}

//****************************************************************************
// Helper Routines
//****************************************************************************

// FIXME: This needs to be rewritten to be consistent with LoadModule
// classes.  But more importantly...

// MAP: This is a temporary and inadequate implementation for building
// the [PC -> src-line] map.  In particular, the map should be built
// with the source-code recovery information that is computed as the
// ScopeTree is construced, esp. the loop nesting information.  (For
// example, perhaps the map should be constructed with the ScopeTree).

static void 
BuildPCToSrcLineMap(PCToSrcLineXMap* map, Procedure* p)
{
  String funcNm   = GetBestFuncName(p->GetName()); 
  String funcLnNm = GetBestFuncName(p->GetLinkName());

  ProcPCToSrcLineXMap* pmap = NULL;
  int pmap_insertCnt = 0;
  
  String theFunc /*= funcNm*/, theFile;
  int funcCnt = 0;
  for (ProcedureInstructionIterator it(*p); it.IsValid(); ++it) {
    Instruction* inst = it.Current();
    Addr pc = inst->GetPC();
    
    // 1. Attempt to find symbolic information
    String func, file;
    suint line;
    p->GetSourceFileInfo(pc, inst->GetOpIndex(), func, file, line);
    func = GetBestFuncName(func);

    if ( !(!func.Empty() && IsValidLine(line)) ) {
      continue; // cannot continue without valid symbolic info
    }    
    if (file.Empty() && !theFile.Empty() // possible replacement...
	&& func == theFunc) { // ...the replacement is valid
      file = theFile; 
    }
    
    // 2. If we are starting a new function, insert old map and create another
    if (func != theFunc) {
      if (pmap_insertCnt > 0) { map->InsertProcInList(pmap); }
      else { delete pmap; }

      pmap_insertCnt = 0;
      Addr strt = ((funcCnt++ == 0) ? p->GetStartAddr() : pc); // FIXME
      pmap = new ProcPCToSrcLineXMap(strt, p->GetEndAddr(), func, file);
    }

    // 3. Update 'pmap'
    if (!pmap) {
      BriefAssertion(false);
    }
    SrcLineX* lineInstance = new SrcLineX(line, 0); // MAP
    pmap->Insert(pc, lineInstance);
    pmap_insertCnt++;
    
    if (theFunc != func) { theFunc = func; }
    if (theFile != file) { theFile = file; }
  }

  // 4. Process last pmap
  if (pmap_insertCnt > 0) { map->InsertProcInList(pmap); }
  else { delete pmap; }
}

