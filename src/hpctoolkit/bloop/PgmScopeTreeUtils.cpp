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
//    PgmScopeTreeUtils.C
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

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "Args.h"
#include "PgmScopeTreeUtils.h"
#include "CodeInfoPtrSet.h"
#include "BloopIRInterface.h"

#include <lib/binutils/LoadModule.h>
#include <lib/binutils/Section.h>
#include <lib/binutils/Procedure.h>
#include <lib/binutils/BinUtils.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/support/StringHashTable.h>
#include <lib/support/PointerStack.h>
#include <lib/support/PtrStack.h>
#include <lib/support/Files.h>
#include <lib/support/Assertion.h>
#include <lib/support/pathfind.h>

#include <OpenAnalysis/CFG/CFG.h>
#include <OpenAnalysis/CFG/OARIFG.h>
#include <OpenAnalysis/CFG/TarjanIntervals.h>

//*************************** Forward Declarations ***************************

// Enabling this flag will permit only procedures with 'interesting'
// information to be added to the ScopeTree
#define BLOOP_SCOPE_TREE_ONLY_CONTAINS_INTERESTING_INFO

const char* OrphanedProcedureFile =
  "~~~bloop:-No-Associated-File-Found-For-These-Procedures";

//#define BLOOP_DEBUG_PROC

// MAP: Temporary and inadequate implementation.  Note more specific
// comments above function def.
void BuildPCToSrcLineMap(PCToSrcLineXMap* map, Procedure* p); 

std::map<CFG::Node*, std::pair<Addr, Addr>* > BlockToPCMap;
typedef std::map<CFG::Node*, std::pair<Addr, Addr>* >::iterator BlockToPCMapIt;

static void CFG_GetStartAndEndAddresses(CFG::Node* n, Addr &start, Addr &end);
static void CFG_BuildBlockToPCMap(CFG*, CFG::Node*, Procedure*, Addr);
static void CFG_ClearBlockToPCMap();

//****************************************************************************
// Set of routines to build a scope tree
//****************************************************************************

const char *PGMdtd =
#include <lib/xml/PGM.dtd.h>

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


PgmScopeTree*
BuildScopeTreeFromExe(Executable* exe, PCToSrcLineXMap* &map,
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
        cerr <<"Building scope tree for [" << p->GetName()  << "] ... ";
      }
      BuildScopeTreeFromProc(fileScope, p);
      if (map) { BuildPCToSrcLineMap(map, p); } // MAP
      if (verboseMode){
        cerr <<"done " << endl;
      }
       
    }
  }

  if (map) { map->Finalize(); } // MAP
  
  if (canonicalPathList.Length() > 0) {
    bool result = FilterFilesFromScopeTree(pgmScopeTree, canonicalPathList);
    BriefAssertion(result); // Should never be false
  }

  if (normalizeScopeTree) {
    bool result = NormalizeScopeTree(pgmScopeTree);
    BriefAssertion(result); // Should never be false
  }

  return pgmScopeTree;
}



#ifdef BLOOP_DEBUG_PROC
bool testProcNow = false;
#endif

ProcScope* 
BuildScopeTreeFromProc(FileScope* enclosingScope, Procedure* p)
{  
  String func, file;
  suint startLn, endLn;
  Instruction* eInst = p->GetLastInst();
  ushort endOp = (eInst) ? eInst->GetOpIndex() : 0;
  
  p->GetSourceFileInfo(p->GetStartAddr(), 0, p->GetEndAddr(), endOp,
		       func, file, startLn, endLn);

  String funcNm   = GetBestFuncName(p->GetName()); 
  String funcLnNm = GetBestFuncName(p->GetLinkName());

  ProcScope* pScope = new ProcScope((const char*)funcNm, enclosingScope,
				    (const char*)funcLnNm, startLn, endLn);

#ifdef BLOOP_DEBUG_PROC
  testProcNow = false;
  suint dbgId = p->GetId(); 

  const char* dbgNm = "__check_eh_spec";
  if (strncmp(funcNm, dbgNm, strlen(dbgNm)) == 0) { testProcNow = true; }
  //if (dbgId == 537) { testProcNow = true; }

  //cout << "==> Processing `" << funcNm << "' (" << dbgId << ") <==\n";
#endif
  
  // -----------------------------------------------------------------  
  // Traverse the Tarjan Tree
  // -----------------------------------------------------------------  
  BloopIRInterface irInterface(p);
  BloopIRStmtIterator stmtIter(*p);
  CFG* g = new CFG(irInterface, &stmtIter, (SymHandle)((const char *)funcNm));
  CFG_BuildBlockToPCMap(g, g->Entry(), p, 0);

  OARIFG tmpRIFG(*g);
  TarjanIntervals tmpTarj(tmpRIFG);

  RIFGNodeId root = tmpRIFG.GetRootNode();

#ifdef BLOOP_DEBUG_PROC
  if (testProcNow) {
    cout << "*** CFG for `" << funcNm << "' ***" << endl;
    cout << "  total blocks: " << g->num_nodes() << endl
	 << "  total edges:  " << g->num_edges() << endl;
    g->dump(cout);

    cout << "*** Tarjan Interval Tree for `" << funcNm << "' ***" << endl;
    tmpTarj.Dump();
    cout << endl;
    cout.flush(); cerr.flush();
  }
#endif 

  // Optimizing compilers may move statments within a loop into one or more 
  // enclosing scopes outside of the original loop (but never past the
  // routine, of course), or, in rare cases, into a nested loop (e.g. to free
  // up registers).  We want to associate stmts within loops to
  // the deepest nested loop in which any instruction deriving from that
  // stmt appears.  
  StringHashTable stmtTable; 

  int numLoops = BuildScopeTreeForInterval(pScope, root, &tmpTarj,
					   g, &stmtTable, 0, &tmpRIFG,
                                           p->GetLoadModule());

  // -----------------------------------------------------------------  
  // Add collected statements to ScopeTree
  // -----------------------------------------------------------------  
  for (StringHashTableIterator it(&stmtTable); it.Current() != NULL; it++) {
    StmtData* sData = (StmtData*)(it.Current()->GetData());
    do {
      BuildScopeTreeFromInst(sData->GetAssocLoop(), sData->GetStartLine(),
			     sData->GetEndLine());
      StmtData *aux = sData;
      sData = sData->GetNext();
      delete aux; // Cleanup
    } while (sData);
    it.Current()->SetData(NULL);
  }
  
#ifdef BLOOP_SCOPE_TREE_ONLY_CONTAINS_INTERESTING_INFO
  // If there are no interesting loops in this procedure, then we
  // don't want it in the Scope Tree
  if (numLoops == 0) {
    pScope->Unlink();
    delete pScope;
    pScope = NULL;
  }
#endif
  
  // Cleanup
  delete g;
  CFG_ClearBlockToPCMap();
  
  return pScope;
}

// Recursively build loops using Tarjan interval analysis
int 
BuildScopeTreeForInterval(CodeInfo* enclosingScope, RIFGNodeId node, 
			  TarjanIntervals *tarj, CFG *g, 
			  StringHashTable *stmtTable, int addStmts,
			  RIFG *tmpRIFG, LoadModule *lm)
{
  int localLoops = 0;
  int kid;
  CFG::Node* crtBlock;
  String func, file;
  Addr startAddr, endAddr;
  suint startLn = UNDEF_LINE, endLn = UNDEF_LINE;
  suint loopsStartLn = UNDEF_LINE, loopsEndLn = UNDEF_LINE;

  CFG::Node* tmpBB = (CFG::Node*)(tmpRIFG->GetRIFGNode(node));
  BriefAssertion(BlockToPCMap.find(tmpBB) != BlockToPCMap.end());
  startAddr = BlockToPCMap[tmpBB]->first;
  endAddr = BlockToPCMap[tmpBB]->second;

  if (addStmts) {
    GatherStmtsFromBB((CFG::Node*)(tmpRIFG->GetRIFGNode(node)),
                      (LoopScope*)enclosingScope, 
		      stmtTable, tarj->Level(node), lm); 
  }

  for ( kid = tarj->TarjInners(node) ; kid != RIFG_NIL ; 
        kid = tarj->TarjNext(kid) ) {
    crtBlock = (CFG::Node*)(tmpRIFG->GetRIFGNode(kid));
    if (tarj->IntervalType(kid) == RI_TARJ_ACYCLIC) { // not a loop head
      if (tarj->TarjNext(kid) == RIFG_NIL) {
        BriefAssertion(BlockToPCMap.find(crtBlock) != BlockToPCMap.end());
        endAddr = BlockToPCMap[crtBlock]->second;
      }
      if (addStmts) {
	GatherStmtsFromBB(crtBlock, (LoopScope*)enclosingScope, 
			  stmtTable, tarj->Level(kid), lm); 
      }
    }
    
    if (tarj->IntervalType(kid) == RI_TARJ_INTERVAL) { // loop head
      LoopScope* lScope = new LoopScope(enclosingScope, UNDEF_LINE,
					UNDEF_LINE, crtBlock->getID());
      int iL = BuildScopeTreeForInterval(lScope, kid, tarj, g, stmtTable, 1,
                                         tmpRIFG, lm);
      localLoops += iL + 1;

      // eraxxon: MAP: -- Add all PC's from loop head/end BB to map
      
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
       
#ifdef BLOOP_SCOPE_TREE_ONLY_CONTAINS_INTERESTING_INFO
      // Remove this loop if we cannot find valid line numbers
      if (!IsValidLine(lScope->BegLine(), lScope->EndLine())) {
	lScope->Unlink();
	delete lScope;
	localLoops--; // valid if done recursively...
      }
#endif
    }

    if (tarj->IntervalType(kid) == RI_TARJ_IRREDUCIBLE) { // not a loop head
      localLoops += BuildScopeTreeForInterval(enclosingScope, kid, tarj, g,
					      stmtTable, addStmts, tmpRIFG,
                                              lm);
    }
  }

  // FIXME: it would probably be better to include opIndices here
  lm->GetSourceFileInfo(startAddr, 0, endAddr, 0, func, file, startLn, endLn);

  if (loopsStartLn != UNDEF_LINE && loopsStartLn < startLn) {
    startLn = loopsStartLn;
  }
  if (loopsEndLn != UNDEF_LINE && loopsEndLn > endLn) {
    endLn = loopsEndLn;
  }
  if (IsValidLine(startLn, endLn)) {
    enclosingScope->SetLineRange(startLn, endLn); // conditional
  }
  
  return localLoops;
}

// Gather stmts from the current basic block.
void 
GatherStmtsFromBB(CFG::Node* bb, LoopScope* lScope,
		  StringHashTable* stmtTable, int level,
		  LoadModule *lm) 
{  
  String func, file;
  suint line;
  StringHashBucket testQuery;
  Addr pc;

  for (CFG::NodeStatementsIterator s_iter(bb); (bool)s_iter; ++s_iter) {
    Instruction *tmpInsn = (Instruction *)((StmtHandle)s_iter);
    pc = tmpInsn->GetPC();
    
    lm->GetSourceFileInfo(pc, tmpInsn->GetOpIndex(), func, file, line); 
    if ( !(!file.Empty() && IsValidLine(line)) ) {
      continue; // cannot continue without valid symbolic info
    }
      
    // Update the stmtTable:
    //   Overwrite any identical stmt entry (file + line_number) already
    //   in the table if the following is true: the entry was added from
    //   an *enclosing* scope (i.e. a lower level).  In this case, some
    //   (but not all) of the instructions for the current statment
    //   were found outside of this deeper loop. 
    String hashKey = file + String(":") + String(line); 
    testQuery.SetKey(hashKey);
    StringHashBucket* qResult = stmtTable->QueryEntry(&testQuery);
    if (qResult) {
      StmtData* sData = (StmtData*)qResult->GetData();
      if (sData->GetLevel() < level) { // update only if found in inner loop
	sData->SetAssocLoop(lScope); // update associated loop
	sData->SetLevel(level); // update deepest level where stmt is present
      }
    } else {
      StmtData* sData = new StmtData(lScope, line, line, level);
      stmtTable->AddEntry(new StringHashBucket(hashKey, sData));
    }
    // eraxxon: MAP: add all PC's for BB to map
  } 
} 

StmtRangeScope* 
BuildScopeTreeFromInst(LoopScope* enclosingScope,
		       long startLine, long endLine)
{
  StmtRangeScope* srScope =
    new StmtRangeScope(enclosingScope, startLine, endLine);
  return srScope;
}


FileScope* 
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
// Routines for normalizing the ScopeTree
//****************************************************************************

bool 
NormalizeScopeTree(PgmScopeTree* pgmScopeTree)
{
  bool pass1 = RemoveOrphanedProcedureRepository(pgmScopeTree);
    
  bool pass2 = MergeOverlappingCode(pgmScopeTree);
  bool pass3 = MergePerfectlyNestedLoops(pgmScopeTree);
  
  bool pass4 = RemoveEmptyFiles(pgmScopeTree);

  return (pass1 && pass2 && pass3 && pass4);
}

// Remove the OrphanedProcedureFile from the tree.
bool 
RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree)
{
  bool noError = true;

  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return noError; }

  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); /* */) {
    BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    it++; // advance iterator -- it is pointing at 'file'

    if (strcmp(file->Name(), OrphanedProcedureFile) == 0) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
    }
  } 
  
  return noError;
}

// When any particular loop or statement overlaps with another [at the
// same level, having the same parent loop], fuse them.  (Loop
// unrolling (start-up, steady-state, wind-down), e.g., can produce
// strange cases of overlapping code).
bool MergeOverlappingCode(ScopeInfo* node);

bool 
MergeOverlappingCode(PgmScopeTree* pgmScopeTree)
{
  return MergeOverlappingCode(pgmScopeTree->GetRoot());
}

bool 
MergeOverlappingCode(ScopeInfo* node)
{
  bool noError = true;
  
  if (!node) { return noError; }
  
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
    noError = noError && MergeOverlappingCode(child);

    // 2. Test for overlapping loops and merge if necessary
    if (child->Type() == ScopeInfo::LOOP) {
      
      // Collect loops in a set that prevents overlapping elements;
      // AddOrMerge may unlink 'child' from tree and delete it.
      (void) mergedSet->AddOrMerge(child); 
    }
  } 
  delete mergedSet;
  delete childSet;
  
  return noError; 
}

// Fuse together a child with a parent when the child is perfectly nested
// in the parent.
bool MergePerfectlyNestedLoops(ScopeInfo* node);

bool 
MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree)
{
  return MergePerfectlyNestedLoops(pgmScopeTree->GetRoot());
}

bool 
MergePerfectlyNestedLoops(ScopeInfo* node)
{
  bool noError = true;
  
  if (!node) { return noError; }
  
  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(node); it.Current(); /* */) {
    CodeInfo* child = dynamic_cast<CodeInfo*>(it.Current()); // always true
    BriefAssertion(child);
    it++; // advance iterator -- it is pointing at 'child'
    
    // 1. Recursively do any merging for this tree's children
    noError = noError && MergePerfectlyNestedLoops(child);

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
    }
  } 
  
  return noError; 
}

bool 
RemoveEmptyFiles(PgmScopeTree* pgmScopeTree)
{
  bool noError = true;

  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return noError; }

  // For each immediate child of this node...
  for (ScopeInfoChildIterator it(pgmScope); it.Current(); /* */) {
    BriefAssertion(((ScopeInfo*)it.Current())->Type() == ScopeInfo::FILE);
    FileScope* file = dynamic_cast<FileScope*>(it.Current()); // always true
    it++; // advance iterator -- it is pointing at 'file'

    // Verify this file has at least one child
    if (file->ChildCount() == 0) {
      file->Unlink(); // unlink 'file' from tree
      delete file;
    }
  } 
  
  return noError;
}

bool 
FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree, String canonicalPathList)
{
  bool noError = true;

  PgmScope* pgmScope = pgmScopeTree->GetRoot();
  if (!pgmScope) { return noError; }
  
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
    }
  } 
  
  return noError;
}

//****************************************************************************
// CFG helper routines
//****************************************************************************

void 
CFG_GetStartAndEndAddresses(CFG::Node* n, Addr &start, Addr &end)
{
  start = 0;
  end = 0;

  // FIXME: It would be a lot faster and easier if we could just grab
  // the first and last statements from a block. Perhaps we'll
  // modify the CFG code, this works for now.
  bool first = true;
  for (CFG::NodeStatementsIterator s_iter(n); (bool)s_iter; ++s_iter) {
    if (first == true) {
      start = ((Instruction *)((StmtHandle)s_iter))->GetPC();
      first = false;
    }
    end = ((Instruction *)((StmtHandle)s_iter))->GetPC();
  }
}

void 
CFG_BuildBlockToPCMap(CFG* g, CFG::Node* n, Procedure* p, Addr _end)
{
  Addr currStart = 0, currEnd = 0;

  // If n is not in the map, it hasn't been visited yet.
  if (BlockToPCMap.find(n) == BlockToPCMap.end()) {
    if (n->empty() == true) {
      // Empty blocks won't have any instructions to get PCs from.
      if (n == g->Entry()) {
        // Set the entry node PCs from the procedure's start address.
        currStart = currEnd = p->GetStartAddr();
      } else {
        // Otherwise, use the end PC propagated from ancestors.
        currStart = currEnd = _end;
      }
    } else {
      // Non-empty blocks will have some instructions to get PCs from.
      CFG_GetStartAndEndAddresses(n, currStart, currEnd);
    }
    BlockToPCMap[n] = new std::pair<Addr, Addr>(currStart, currEnd);

    // Visit descendents.
    for (CFG::SinkNodesIterator sink(n); (bool)sink; ++sink) {
      CFG::Node* child = dynamic_cast<CFG::Node*>((DGraph::Node*)sink);
      CFG_BuildBlockToPCMap(g, child, p, currEnd);
    }
  }
}

void 
CFG_ClearBlockToPCMap()
{
  BlockToPCMapIt it;
  for (it = BlockToPCMap.begin(); it != BlockToPCMap.end(); ++it) {
    delete (*it).second; // std::pair<Addr, Addr>*
  }
  BlockToPCMap.clear();
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

void 
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

