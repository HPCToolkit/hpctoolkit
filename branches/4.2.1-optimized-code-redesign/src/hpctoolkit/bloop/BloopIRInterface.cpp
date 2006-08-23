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
//   A derivation of the IR interface for the ISA (disassembler) class
//   of bloop.
//
//   Note: many stubs still exist.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "BloopIRInterface.hpp"

//*************************** Forward Declarations **************************

#include <iostream>
using std::cout;
using std::endl;
using std::hex;
using std::dec;

//****************************************************************************

//***************************************************************************
// Iterators
//***************************************************************************

//***************************************************************************
// Abstract Interfaces
//***************************************************************************

//-----------------------------------------------------------------------------
// BloopIRInterface
//-----------------------------------------------------------------------------

// BloopIRInterface: We assume each instantiation of the IRInterface
// represents one procedure.  We then need to find all the possible
// branch targets.
BloopIRInterface::BloopIRInterface (Procedure *_p) 
  : proc(_p)
{
  branchTargetSet.clear();
  for (ProcedureInstructionIterator pii(*proc); pii.IsValid(); ++pii) {
    Instruction *insn = pii.Current();
    VMA curr_oppc = pii.CurrentVMA(); // the 'operation VMA'
    
    // If this insn is a branch, record its target address in
    // the branch target set.
    ISA::InstDesc d = insn->GetDesc();
    if (d.IsBrRel()) {
      branchTargetSet.insert(insn->GetTargetVMA(insn->GetVMA()));
    }
  }
}


BloopIRInterface::~BloopIRInterface()
{ 
  branchTargetSet.clear();
}


//-------------------------------------------------------------------------
// IRHandlesIRInterface
//-------------------------------------------------------------------------

std::string 
BloopIRInterface::toString(const OA::ProcHandle h)
{ 
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::StmtHandle h)
{ 
  std::ostringstream oss;
  if (h.hval() == 0) {
    oss << "StmtHandle(0)";
  } 
  else {
    dump(h, oss);
  }
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::ExprHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::OpHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::MemRefHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::SymHandle h) 
{
  const char* sym = IRHNDL_TO_TY(h, const char*);
  std::string s(sym);
  return s;
}


std::string 
BloopIRInterface::toString(const OA::ConstSymHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


std::string 
BloopIRInterface::toString(const OA::ConstValHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


void 
BloopIRInterface::dump(OA::StmtHandle stmt, std::ostream& os)
{
  Instruction *insn = IRHNDL_TO_TY(stmt, Instruction*);
 
  // Currently, we just print the instruction type.  What we really
  // want is a textual disassembly of the instruction from the
  // disassembler.

  VMA pc = insn->GetVMA();
  ISA::InstDesc d = insn->GetDesc();

  // Output pc and descriptor
  cout << hex << "0x" << pc << dec << ": " << d.ToString();
  
  // Output other qualifiers
  if (branchTargetSet.find(pc) != branchTargetSet.end()) {
    cout << " [branch target]";
  }
  if (d.IsBrRel()) {
    VMA targ = insn->GetTargetVMA(pc);
    os << " <" << hex << "0x" << targ << dec << ">";
    if (proc->IsIn(targ) == false) {
      cout << " [out of procedure -- treated as SIMPLE]";
    }
  } 
  else if (d.IsBrInd()) {
    os << " <register>";
  }
}


void 
BloopIRInterface::dump(OA::MemRefHandle h, std::ostream& os)
{
  BriefAssertion(0);
}


void 
BloopIRInterface::currentProc(OA::ProcHandle p)
{
  BriefAssertion(0);
}


//---------------------------------------------------------------------------
// CFGIRInterfaceDefault
//---------------------------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator> 
BloopIRInterface::procBody(OA::ProcHandle h)
{
  Procedure* p = IRHNDL_TO_TY(h, Procedure*);
  BriefAssertion(p == proc);
  OA::OA_ptr<OA::IRRegionStmtIterator> it;
  it = new BloopIRRegionStmtIterator(*p);
  return it;
}


//----------------------------------------------------------
// Statements: General
//----------------------------------------------------------

// Translate a ISA statement type into a IRStmtType.
OA::CFG::IRStmtType
BloopIRInterface::getCFGStmtType(OA::StmtHandle h) 
{
  OA::CFG::IRStmtType ty;
  Instruction *insn = IRHNDL_TO_TY(h, Instruction*);
  VMA br_targ = 0;

  ISA::InstDesc d = insn->GetDesc();
  if (d.IsBrUnCondRel()) {
    // Unconditional jump. If the branch targets a PC outside of its
    // procedure, then we just ignore it.  For bloop this is fine
    // since the branch won't create any loops.
    br_targ = insn->GetTargetVMA(insn->GetVMA());
    if (proc->IsIn(br_targ)) {
      ty = OA::CFG::UNCONDITIONAL_JUMP;
    } 
    else {
      ty = OA::CFG::SIMPLE;
    }
  } 
  else if (d.IsBrUnCondInd()) {
    // Unconditional jump (indirect).
    //ty = OA::CFG::UNCONDITIONAL_JUMP_I; // FIXME: Turbo hack, temporary. 
    ty = OA::CFG::SIMPLE;
  } 
  else if (d.IsBrCondRel()) {
    // Unstructured two-way branches. If the branch targets a PC
    // outside of its procedure, then we just ignore it.  For bloop
    // this is fine since the branch won't create any loops.
    br_targ = insn->GetTargetVMA(insn->GetVMA());
    if (proc->IsIn(br_targ)) {
      ty = OA::CFG::USTRUCT_TWOWAY_CONDITIONAL_T;
    }
    else {
      ty = OA::CFG::SIMPLE;
    }
  } 
#if 0  
  // FIXME: ISA currently assumes every cbranch is "branch on true".
  else if (d.br_cond_rel_on_false) {
    ty = OA::CFG::USTRUCT_TWOWAY_CONDITIONAL_F;
  }
#endif
  else if (d.IsBrCondInd()) {
    ty = OA::CFG::SIMPLE; // FIXME: Turbo hack, temporary. 
  } 
  else if (d.IsSubrRet()) {
    // Return statement.
    ty = OA::CFG::RETURN;
  }
  else {
    // Simple statements.
    ty = OA::CFG::SIMPLE;
  }
  
  return ty;
}


OA::StmtLabel
BloopIRInterface::getLabel(OA::StmtHandle h)
{
  OA::StmtLabel lbl = 0;
  Instruction *insn = IRHNDL_TO_TY(h, Instruction*);
  if (branchTargetSet.find(insn->GetVMA()) != branchTargetSet.end()) {
    lbl = insn->GetVMA();
  } 
  return lbl;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::getFirstInCompound(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::OA_ptr<BloopIRRegionStmtIterator>();
}


//--------------------------------------------------------
// Loops
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::loopBody(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::OA_ptr<OA::IRRegionStmtIterator>();
}


OA::StmtHandle
BloopIRInterface::loopHeader(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::StmtHandle(0);
}


OA::StmtHandle
BloopIRInterface::getLoopIncrement(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::StmtHandle(0);
}


bool
BloopIRInterface::loopIterationsDefinedAtEntry(OA::StmtHandle h)
{
  return false;
}


//--------------------------------------------------------
// Structured two-way conditionals
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::trueBody(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::OA_ptr<BloopIRRegionStmtIterator>();
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::elseBody(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::OA_ptr<BloopIRRegionStmtIterator>();
}


//--------------------------------------------------------
// Structured multiway conditionals
//--------------------------------------------------------

int
BloopIRInterface::numMultiCases(OA::StmtHandle h)
{
  BriefAssertion(0);
  return -1;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::multiBody(OA::StmtHandle h, int bodyIndex)
{
  BriefAssertion(0);
  return OA::OA_ptr<BloopIRRegionStmtIterator>();
}


bool
BloopIRInterface::isBreakImplied(OA::StmtHandle h)
{
  BriefAssertion(0);
  return false;
}


bool
BloopIRInterface::isCatchAll(OA::StmtHandle h, int bodyIndex)
{
  BriefAssertion(0);
  return false;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BloopIRInterface::getMultiCatchall(OA::StmtHandle h)
{
  BriefAssertion(0);
  return OA::OA_ptr<BloopIRRegionStmtIterator>();
}


OA::ExprHandle
BloopIRInterface::getSMultiCondition(OA::StmtHandle h, int bodyIndex)
{
  BriefAssertion(0);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Unstructured two-way conditionals
//--------------------------------------------------------

OA::StmtLabel
BloopIRInterface::getTargetLabel(OA::StmtHandle h, int n)
{
  OA::StmtLabel lbl = 0;
  Instruction *insn = IRHNDL_TO_TY(h, Instruction*);
  ISA::InstDesc d = insn->GetDesc();
  if (d.IsBrRel()) {
    lbl = insn->GetTargetVMA(insn->GetVMA());
  } 
  else {
    lbl = 0; // FIXME: We're seeing indirect branches.
  }
  return lbl;
}


//--------------------------------------------------------
// Unstructured multi-way conditionals
//--------------------------------------------------------

int
BloopIRInterface::numUMultiTargets(OA::StmtHandle h)
{ 
  BriefAssertion(0);
  return -1;
}


OA::StmtLabel
BloopIRInterface::getUMultiTargetLabel(OA::StmtHandle h, int targetIndex)
{
  BriefAssertion(0);
  return OA::StmtLabel(0);
}


OA::StmtLabel
BloopIRInterface::getUMultiCatchallLabel(OA::StmtHandle h)
{ 
  BriefAssertion(0);
  return OA::StmtLabel(0);
}


OA::ExprHandle
BloopIRInterface::getUMultiCondition(OA::StmtHandle h, int targetIndex)
{
  BriefAssertion(0);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Special
//--------------------------------------------------------

int
BloopIRInterface::numberOfDelaySlots(OA::StmtHandle h)
{
  Instruction *insn = IRHNDL_TO_TY(h, Instruction*);
  return insn->GetNumDelaySlots();
}


//--------------------------------------------------------
// Symbol Handles
//--------------------------------------------------------

OA::SymHandle 
BloopIRInterface::getProcSymHandle(OA::ProcHandle h)
{ 
  Procedure* p = IRHNDL_TO_TY(h, Procedure*);
  BriefAssertion(p == proc);
  std::string& name = p->GetName(); // this gives us persistent data!
  const char* nm = name.c_str();
  return TY_TO_IRHNDL(nm, OA::SymHandle);
}

