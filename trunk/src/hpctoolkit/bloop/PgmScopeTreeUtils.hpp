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
//    PgmScopeTreeUtils.H
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef PgmScopeTreeUtils_H 
#define PgmScopeTreeUtils_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <OpenAnalysis/CFG/CFG.h>
#include <OpenAnalysis/CFG/RIFG.h>

#include <include/general.h> 

#include "PgmScopeTree.h"
#include <lib/binutils/LoadModule.h>
#include <lib/support/StringHashTable.h>
#include <lib/support/PointerStack.h>

//*************************** Forward Declarations ***************************

class PCToSrcLineXMap;

//*************************** Forward Declarations ***************************

// Functions to build and dump a scope tree from an Executable

void
WriteScopeTree(std::ostream& os, PgmScopeTree* pgmScopeTree,
	       bool prettyPrint = true);

PgmScopeTree*
BuildScopeTreeFromExe(Executable* exe, PCToSrcLineXMap* &map,
		      String canonicalPathList = "",
		      bool normalizeScopeTree = true);

//*************************** Forward Declarations ***************************

// Private functions for building a scope tree

ProcScope*
BuildScopeTreeFromProc(FileScope* enclosingScope, Procedure* p); 


int
BuildScopeTreeForInterval(CodeInfo* enclosingScope, RIFGNodeId node,
			  TarjanIntervals *tarj, CFG *g,
			  StringHashTable *stmtTable, int,
                          RIFG *tmpRIFG, LoadModule *lm);

void
GatherStmtsFromBB(CFG::Node* head_bb, LoopScope* lScope,
		  StringHashTable* stmtTable, int level,
                  LoadModule *lm);

StmtRangeScope*
BuildScopeTreeFromInst(LoopScope* enclosingScope, 
		       long startLine, long endLine);

FileScope*
FindOrCreateFileNode(PgmScope* pgmScopeTree, Procedure* p);

// Functions to 'normalize' a scope tree

bool NormalizeScopeTree(PgmScopeTree* pgmScopeTree);
bool RemoveOrphanedProcedureRepository(PgmScopeTree* pgmScopeTree);
bool MergeOverlappingCode(PgmScopeTree* pgmScopeTree);
bool MergePerfectlyNestedLoops(PgmScopeTree* pgmScopeTree);
bool RemoveEmptyFiles(PgmScopeTree* pgmScopeTree);

bool FilterFilesFromScopeTree(PgmScopeTree* pgmScopeTree,
			      String canonicalPathList);

//****************************************************************************

// Simple class for data associated with a statement
class StmtData {
public:
  StmtData(LoopScope* _assocLoop = NULL, long _startLine = 0,
	   long _endLine = 0, int _level = 0)
    : assocLoop(_assocLoop), startLine(_startLine), endLine(_endLine), 
      level(_level), next(NULL) { }

  ~StmtData() { /* owns no data */ }

  LoopScope* GetAssocLoop() { return assocLoop; }
  long GetStartLine() { return startLine; }
  long GetEndLine() { return endLine; }
  int GetLevel() { return level; }
  StmtData *GetNext() { return next; }

  void SetAssocLoop(LoopScope* _assocLoop) { assocLoop = _assocLoop; }
  void SetStartLine(long _startLine) { startLine = _startLine; }
  void SetEndLine(long _endLine) { endLine = _endLine; }
  void SetLevel(int _level) { level = _level; }
  void SetNext(StmtData *_next) { next = _next; }
  
private:
  LoopScope* assocLoop; // associated ScopeTree loop
  suint startLine, endLine;
  int level;
  StmtData *next;
};

#endif
