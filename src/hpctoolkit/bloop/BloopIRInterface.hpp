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
//    BloopIRInterface.h
//
// Purpose:
//   A derivation of the IR interface for the ISA (disassembler) class
//   of bloop.
//
//   Note: many stubs still exist.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef BloopIRInterface_h
#define BloopIRInterface_h

//************************* System Include Files ****************************

#include <list>
#include <set>

//*************************** User Include Files ****************************

// OpenAnalysis headers.
// Use OA_IRHANDLETYPE_SZ64: size of bfd_vma/Addr
#include <OpenAnalysis/Utils/DGraph.h>
#include <OpenAnalysis/Interface/IRInterface.h>
 
#include <lib/ISA/ISA.h>
#include <lib/binutils/Instruction.h>
#include <lib/binutils/Procedure.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

class BloopIRStmtIterator: public IRStmtIterator {
public:
  BloopIRStmtIterator (Procedure &_p) : pii(_p) {  };
  ~BloopIRStmtIterator () {};

  StmtHandle Current () { return (StmtHandle)(pii.Current()); };
  bool IsValid () { return pii.IsValid(); };
  void operator++ () { ++pii; };
private:
  ProcedureInstructionIterator pii;
};


class BloopIRUseDefIterator: public IRUseDefIterator {
public:
  BloopIRUseDefIterator (Instruction *insn, int uses_or_defs);
  BloopIRUseDefIterator () { BriefAssertion (0); }
  ~BloopIRUseDefIterator () {};

  LeafHandle Current () { return 0; };
  bool IsValid () { return false; };
  void operator++ () {  };
private:
};

//*************************** Forward Declarations ***************************

class BloopIRInterface : public IRInterface {
public:
  BloopIRInterface (Procedure *_p);
  BloopIRInterface () { BriefAssertion(0); }
  ~BloopIRInterface () {}

  //------------------------------
  // General - all statement types
  //------------------------------
  IRStmtType GetStmtType (StmtHandle);
  StmtLabel GetLabel (StmtHandle);

  //------------------------------
  // For procedures, compound statements. 
  //------------------------------
  IRStmtIterator *ProcBody(ProcHandle h) 
    { BriefAssertion(false); return NULL; }
  IRStmtIterator *GetFirstInCompound (StmtHandle h);

  //------------------------------
  // Loops
  //------------------------------
  IRStmtIterator *LoopBody(StmtHandle h);
  StmtHandle LoopHeader (StmtHandle h);
  bool LoopIterationsDefinedAtEntry (StmtHandle h);
  ExprHandle GetLoopCondition (StmtHandle h); 
  StmtHandle GetLoopIncrement (StmtHandle h);

  //------------------------------
  // invariant: a two-way conditional or a multi-way conditional MUST provide
  // provided either a target, or a target label
  //------------------------------

  //------------------------------
  // Structured two-way conditionals
  //------------------------------
  IRStmtIterator *TrueBody (StmtHandle h);
  IRStmtIterator *ElseBody (StmtHandle h);

  //------------------------------
  // Structured multiway conditionals
  //------------------------------
  int NumMultiCases (StmtHandle h);
  // condition for multi body 
  ExprHandle GetSMultiCondition (StmtHandle h, int bodyIndex);
  // multi-way beginning expression
  ExprHandle GetMultiExpr (StmtHandle h);
  IRStmtIterator *MultiBody (StmtHandle h, int bodyIndex);
  bool IsBreakImplied (StmtHandle multicond);
  IRStmtIterator *GetMultiCatchall (StmtHandle h);

  //------------------------------
  // Unstructured two-way conditionals: 
  //------------------------------
  // two-way branch, loop continue
  StmtLabel  GetTargetLabel (StmtHandle h, int n);
  ExprHandle GetCondition (StmtHandle h);

  //------------------------------
  // Unstructured multi-way conditionals
  //------------------------------
  int NumUMultiTargets (StmtHandle h);
  StmtLabel GetUMultiTargetLabel (StmtHandle h, int targetIndex);
  StmtLabel GetUMultiCatchallLabel (StmtHandle h);
  ExprHandle GetUMultiCondition (StmtHandle h, int targetIndex);

  //------------------------------
  // Special
  //------------------------------
  bool ParallelWithSuccessor(StmtHandle h) { return false; }

  // Given an unstructured branch/jump statement, return the number
  // of delay slots.
  int NumberOfDelaySlots(StmtHandle h);
 
  //------------------------------
  // Obtain uses and defs
  //------------------------------
  IRProcCallIterator *GetProcCalls(StmtHandle h) 
    { BriefAssertion(false); return NULL; }
  IRUseDefIterator *GetUses (StmtHandle h);
  IRUseDefIterator *GetDefs (StmtHandle h);

  //------------------------------
  // Symbol Handles
  //------------------------------
  SymHandle GetProcSymHandle(ProcHandle h) { return (SymHandle)0; }
  SymHandle GetSymHandle (LeafHandle vh) { return (SymHandle)0; }
  const char *GetSymNameFromSymHandle (SymHandle sh) { return "<no-sym>"; }

  //------------------------------
  // Debugging
  //------------------------------
  void PrintLeaf (LeafHandle vh, ostream & os) { };
  void Dump (StmtHandle stmt, ostream& os);

private:
  Procedure *proc;
  std::set<Addr> branchTargetSet;
};

#endif // BloopIRInterface_h
