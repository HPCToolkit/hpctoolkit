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
//    BloopIRInterface.C
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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "BloopIRInterface.h"

//*************************** Forward Declarations **************************

using std::cout;
using std::endl;
using std::hex;
using std::dec;

//****************************************************************************

// See OpenAnalysis/Interface/IRInterface.h for more documentation on
// these functions.

//-----------------------------------------------------------------------------
// BloopIRUseDefIterator
//-----------------------------------------------------------------------------

// The ISA_IRUseDefIterator constructor.
BloopIRUseDefIterator::BloopIRUseDefIterator (Instruction *insn, int u_or_d)
{
  // FIXME: stub
}

//-----------------------------------------------------------------------------
// BloopIRInterface
//-----------------------------------------------------------------------------

// When this constructor is called, we assume that this instantiation
// of the IRInterface represents a procedure.  We then need to find
// all the possible branch targets.
BloopIRInterface::BloopIRInterface (Procedure *_p) : proc(_p)
{
  ProcedureInstructionIterator pii(*_p);
  branchTargetSet.clear();
  while (pii.IsValid()) {
    Instruction *insn = pii.Current();
    Addr curr_pc = pii.CurrentPC();

    // If this insn is a branch, record its target address in
    // the branch target set.
    ISA::InstDesc d = insn->GetDesc();
    if (d.IsBrRel()) {
      branchTargetSet.insert(insn->GetTargetAddr(curr_pc));
    }
    ++pii;
  }
}

// Translate a ISA statement type into a IRStmtType.
IRStmtType
BloopIRInterface::GetStmtType (StmtHandle h) 
{
  IRStmtType ty;
  Instruction *insn = (Instruction *) h;
  StmtLabel br_targ = 0;

  ISA::InstDesc d = insn->GetDesc();
  if (d.IsBrUnCondRel()) {
    // Unconditional jump. If the branch targets a PC outside of its
    // procedure, then we just ignore it.  For bloop this is fine
    // since the branch won't create any loops.
    br_targ = (StmtLabel) (insn->GetTargetAddr(insn->GetPC()));
    if (proc->IsIn(br_targ)) {
      ty = UNCONDITIONAL_JUMP;
    } else {
      ty = SIMPLE;
    }
  } else if (d.IsBrUnCondInd()) {
    // Unconditional jump (indirect).
    // FIXME: Turbo hack, temporary measure. 
    //ty = UNCONDITIONAL_JUMP_I;
    ty = SIMPLE;
  } else if (d.IsBrCondRel()) {
    // Unstructured two-way branches. If the branch targets a PC
    // outside of its procedure, then we just ignore it.  For bloop
    // this is fine since the branch won't create any loops.
    br_targ = (StmtLabel) (insn->GetTargetAddr(insn->GetPC()));
    if (proc->IsIn(br_targ)) {
      ty = USTRUCT_TWOWAY_CONDITIONAL_T;
    } else {
      ty = SIMPLE;
    }
  } else if (d.IsBrCondInd()) {
    // FIXME: Turbo hack, temporary measure. 
    ty = SIMPLE;
#if 0  // FIXME: ISA currently assumes every cbranch is "branch on true".
  } else if (OPR_FALSEBR) {
    ty = USTRUCT_TWOWAY_CONDITIONAL_F;
#endif
  } else if (d.IsSubrRet()) {
    // Return statement.
    ty = RETURN;
  } else {
    // Simple statements.
    ty = SIMPLE;
  }
  
  return ty;
}

// Given a statement, return the label associated with it (or NULL if none).
StmtLabel
BloopIRInterface::GetLabel (StmtHandle h)
{
  Instruction *insn = (Instruction *) h;
  if (branchTargetSet.find(insn->GetPC()) != branchTargetSet.end()) {
    return (StmtLabel) (insn->GetPC());
  } else {
    return 0;
  }
}


//-----------------------------------------------------------------------------
// For procedures, compound statements. 
//-----------------------------------------------------------------------------

// Given a compound statement, return an IRStmtIterator* for the statements.
IRStmtIterator*
BloopIRInterface::GetFirstInCompound (StmtHandle h)
{
  BriefAssertion (0);
  return NULL;
}


//-----------------------------------------------------------------------------
// Loops
//-----------------------------------------------------------------------------

IRStmtIterator*
BloopIRInterface::LoopBody (StmtHandle h)
{
  BriefAssertion (0);
  return NULL;
}

bool
BloopIRInterface::LoopIterationsDefinedAtEntry (StmtHandle h)
{
  return false;
}


// The loop header is the initialization block.
StmtHandle
BloopIRInterface::LoopHeader (StmtHandle h)
{
  BriefAssertion(0);
  return 0;
}


ExprHandle
BloopIRInterface::GetLoopCondition (StmtHandle h)
{
  BriefAssertion(0);
  return 0;
}


StmtHandle
BloopIRInterface::GetLoopIncrement (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


//-----------------------------------------------------------------------------
// Structured two-way conditionals
//
// FIXME: Is GetCondition for unstructured conditionals also?  It is currently
// implemented that way.
//-----------------------------------------------------------------------------

ExprHandle
BloopIRInterface::GetCondition (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


IRStmtIterator*
BloopIRInterface::TrueBody (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


IRStmtIterator*
BloopIRInterface::ElseBody (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


// Given an unstructured branch/jump statement, return the number
// of delay slots.
int
BloopIRInterface::NumberOfDelaySlots(StmtHandle h)
{
  Instruction *insn = (Instruction *) h;
  return insn->GetNumDelaySlots();
}


//-----------------------------------------------------------------------------
// Structured multiway conditionals
//-----------------------------------------------------------------------------

int
BloopIRInterface::NumMultiCases (StmtHandle h)
{
  BriefAssertion (0);
  return -1;
}


ExprHandle
BloopIRInterface::GetSMultiCondition (StmtHandle h, int bodyIndex)
{
  BriefAssertion (0);
  return 0;
}


ExprHandle
BloopIRInterface::GetMultiExpr (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


// I'm assuming bodyIndex is 0..n-1.
IRStmtIterator*
BloopIRInterface::MultiBody (StmtHandle h, int bodyIndex)
{
  BriefAssertion (0);
  return 0;
}


IRStmtIterator*
BloopIRInterface::GetMultiCatchall (StmtHandle h)
{
  BriefAssertion (0);
  return 0;
}


bool
BloopIRInterface::IsBreakImplied (StmtHandle multicond)
{
  BriefAssertion (0);
  return false;
}


//-----------------------------------------------------------------------------
// Unstructured two-way conditionals: 
//-----------------------------------------------------------------------------

// Unstructured two-way branch (future loop continue?)
StmtLabel
BloopIRInterface::GetTargetLabel (StmtHandle h, int n)
{
  Instruction *insn = (Instruction *) h;
  ISA::InstDesc d = insn->GetDesc();
  if (d.IsBrRel()) {
    return (StmtLabel) (insn->GetTargetAddr(insn->GetPC()));
  } else {
    // FIXME: We're seeing indirect branches.
    return (StmtLabel) 0;
  }
}


//-----------------------------------------------------------------------------
// Unstructured multiway conditionals
//-----------------------------------------------------------------------------

// Given an unstructured multi-way branch, return the number of targets.
// The count does not include the optional default/catchall target.
int
BloopIRInterface::NumUMultiTargets (StmtHandle h)
{ 
  BriefAssertion (0);
  return -1;
}


// Given an unstructured multi-way branch, return the label of the target
// statement at 'targetIndex'. The n targets are indexed [0..n-1]. 
StmtLabel
BloopIRInterface::GetUMultiTargetLabel (StmtHandle h, int targetIndex)
{
  BriefAssertion (0);
  return 0;
}


// Given an unstructured multi-way branch, return the label of the optional
// default/catchall target. Return 0 if no default target.
StmtLabel
BloopIRInterface::GetUMultiCatchallLabel (StmtHandle h)
{ 
  BriefAssertion (0);
  return 0;
}


// Given an unstructured multi-way branch, return the condition expression
// corresponding to target 'targetIndex'. The n targets are indexed [0..n-1].
ExprHandle
BloopIRInterface::GetUMultiCondition (StmtHandle h, int targetIndex)
{
  // FIXME: It isn't yet decided whether or not this function is needed in
  // the IR interface.
  BriefAssertion (0);
  return 0;
}


//-----------------------------------------------------------------------------
// Obtain uses and defs
//-----------------------------------------------------------------------------

IRUseDefIterator*
BloopIRInterface::GetUses (StmtHandle h)
{
  Instruction *insn = (Instruction *) h;
  return new BloopIRUseDefIterator (insn, IRUseDefIterator::Uses);
}


IRUseDefIterator*
BloopIRInterface::GetDefs (StmtHandle h)
{
  Instruction *insn = (Instruction *) h;
  return new BloopIRUseDefIterator (insn, IRUseDefIterator::Defs);
}


// FIXME: Hack.  Only used from CFG::ltnode.
#if 0
const char*
IRGetSymNameFromLeafHandle(LeafHandle vh)
{
  BriefAssertion (0);
  return NULL;
}
#endif

//-----------------------------------------------------------------------------
// Debugging
//-----------------------------------------------------------------------------

void BloopIRInterface::Dump (StmtHandle stmt, ostream& os)
{
  Instruction *insn = (Instruction *) stmt;
 
  // Currently, we just print the instruction type.
  // What we really want is a textual disassembly of the instruction
  // from the disassembler.

  if (branchTargetSet.find(insn->GetPC()) != branchTargetSet.end()) {
    cout << hex << insn->GetPC() << ": ";
  }

  ISA::InstDesc d = insn->GetDesc();
  const char* str = d.ToString();
  StmtLabel br_targ = 0;

  if (d.IsBrRel()) {
      br_targ = (StmtLabel) (insn->GetTargetAddr(insn->GetPC()));
      os << str << " <" << hex << br_targ << ">";
      if (proc->IsIn(br_targ) == false) {
        cout << "[out of procedure -- treated as SIMPLE]";
      }
  } else if (d.IsBrInd()) {
    os << str << " <register>";
  } else {
    os << str;
  }
  os << dec;
}

