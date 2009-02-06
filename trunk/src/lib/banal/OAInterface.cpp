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

#include <iostream>
using std::cout;
using std::endl;
using std::hex;
using std::dec;
using std::showbase;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "OAInterface.hpp"

#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
using namespace BinUtil;

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// Iterators
//***************************************************************************

//***************************************************************************
// Abstract Interfaces
//***************************************************************************

//-----------------------------------------------------------------------------
// OAInterface
//-----------------------------------------------------------------------------

// OAInterface: We assume each instantiation of the IRInterface
// represents one procedure.  We then need to find all the possible
// branch targets.
banal::OAInterface::OAInterface (Proc* proc)
  : m_proc(proc)
{
  m_branchTargetSet.clear();
  for (ProcInsnIterator pii(*m_proc); pii.IsValid(); ++pii) {
    Insn* insn = pii.Current();
    //VMA curr_oppc = pii.CurrentVMA(); // the 'operation VMA'
    
    // If this insn is a branch, record its target address in
    // the branch target set.
    ISA::InsnDesc d = insn->desc();
    if (d.IsBrRel()) {
      m_branchTargetSet.insert(normalizeTarget(insn->targetVMA(insn->vma())));
    }
  }
}


banal::OAInterface::~OAInterface()
{ 
  m_branchTargetSet.clear();
}


//-------------------------------------------------------------------------
// IRHandlesIRInterface
//-------------------------------------------------------------------------

string 
banal::OAInterface::toString(const OA::ProcHandle h)
{ 
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
banal::OAInterface::toString(const OA::CallHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string 
banal::OAInterface::toString(const OA::StmtHandle h)
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


string 
banal::OAInterface::toString(const OA::ExprHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string 
banal::OAInterface::toString(const OA::OpHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string 
banal::OAInterface::toString(const OA::MemRefHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string 
banal::OAInterface::toString(const OA::SymHandle h) 
{
  const char* sym = IRHNDL_TO_TY(h, const char*);
  string s(sym);
  return s;
}


string 
banal::OAInterface::toString(const OA::ConstSymHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string 
banal::OAInterface::toString(const OA::ConstValHandle h) 
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


void 
banal::OAInterface::dump(OA::StmtHandle stmt, std::ostream& os)
{
  Insn* insn = IRHNDL_TO_TY(stmt, Insn*);
 
  // Currently, we just print the instruction type.  What we really
  // want is a textual disassembly of the instruction from the
  // disassembler.

  VMA pc = insn->vma();
  ISA::InsnDesc d = insn->desc();

  // Output pc and descriptor
  os << showbase << hex << pc << dec << ": " << d.ToString();
  
  // Output other qualifiers
  if (m_branchTargetSet.find(pc) != m_branchTargetSet.end()) {
    os << " [branch target]";
  }
  if (d.IsBrRel()) {
    VMA targ = insn->targetVMA(pc);
    os << " <" << hex << targ << dec << ">";
    if (m_proc->isIn(targ) == false) {
      os << " [out of procedure -- treated as SIMPLE]";
    }
  } 
  else if (d.IsBrInd()) {
    os << " <register>";
  }
}


void 
banal::OAInterface::dump(OA::MemRefHandle h, std::ostream& os)
{
  DIAG_Die(DIAG_Unimplemented);
}


void 
banal::OAInterface::currentProc(OA::ProcHandle p)
{
  DIAG_Die(DIAG_Unimplemented);
}


//---------------------------------------------------------------------------
// CFGIRInterfaceDefault
//---------------------------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator> 
banal::OAInterface::procBody(OA::ProcHandle h)
{
  Proc* p = IRHNDL_TO_TY(h, Proc*);
  DIAG_Assert(p == m_proc, "");
  OA::OA_ptr<OA::IRRegionStmtIterator> it;
  it = new RegionStmtIterator(*p);
  return it;
}


//----------------------------------------------------------
// Statements: General
//----------------------------------------------------------

// Translate a ISA statement type into a IRStmtType.
OA::CFG::IRStmtType
banal::OAInterface::getCFGStmtType(OA::StmtHandle h) 
{
  OA::CFG::IRStmtType ty;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  VMA br_targ = 0;

  ISA::InsnDesc d = insn->desc();
  if (d.IsBrUnCondRel()) {
    // Unconditional jump. If the branch targets a PC outside of its
    // procedure, then we just ignore it.  For bloop this is fine
    // since the branch won't create any loops.
    br_targ = insn->targetVMA(insn->vma());
    if (m_proc->isIn(br_targ)) {
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
    br_targ = insn->targetVMA(insn->vma());
    if (m_proc->isIn(br_targ)) {
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
banal::OAInterface::getLabel(OA::StmtHandle h)
{
  OA::StmtLabel lbl = 0;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  if (m_branchTargetSet.find(insn->vma()) != m_branchTargetSet.end()) {
    lbl = insn->vma();
  } 
  return lbl;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::getFirstInCompound(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


//--------------------------------------------------------
// Loops
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::loopBody(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<OA::IRRegionStmtIterator>();
}


OA::StmtHandle
banal::OAInterface::loopHeader(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtHandle(0);
}


OA::StmtHandle
banal::OAInterface::getLoopIncrement(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtHandle(0);
}


bool
banal::OAInterface::loopIterationsDefinedAtEntry(OA::StmtHandle h)
{
  return false;
}


//--------------------------------------------------------
// Structured two-way conditionals
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::trueBody(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::elseBody(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


//--------------------------------------------------------
// Structured multiway conditionals
//--------------------------------------------------------

int
banal::OAInterface::numMultiCases(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return -1;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::multiBody(OA::StmtHandle h, int bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


bool
banal::OAInterface::isBreakImplied(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return false;
}


bool
banal::OAInterface::isCatchAll(OA::StmtHandle h, int bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return false;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
banal::OAInterface::getMultiCatchall(OA::StmtHandle h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


OA::ExprHandle
banal::OAInterface::getSMultiCondition(OA::StmtHandle h, int bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Unstructured two-way conditionals
//--------------------------------------------------------

OA::StmtLabel
banal::OAInterface::getTargetLabel(OA::StmtHandle h, int n)
{
  OA::StmtLabel lbl = 0;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  ISA::InsnDesc d = insn->desc();
  if (d.IsBrRel()) {
    lbl = normalizeTarget(insn->targetVMA(insn->vma()));
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
banal::OAInterface::numUMultiTargets(OA::StmtHandle h)
{ 
  DIAG_Die(DIAG_Unimplemented);
  return -1;
}


OA::StmtLabel
banal::OAInterface::getUMultiTargetLabel(OA::StmtHandle h, int targetIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtLabel(0);
}


OA::StmtLabel
banal::OAInterface::getUMultiCatchallLabel(OA::StmtHandle h)
{ 
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtLabel(0);
}


OA::ExprHandle
banal::OAInterface::getUMultiCondition(OA::StmtHandle h, int targetIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Special
//--------------------------------------------------------

int
banal::OAInterface::numberOfDelaySlots(OA::StmtHandle h)
{
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  return insn->numDelaySlots();
}


//--------------------------------------------------------
// Symbol Handles
//--------------------------------------------------------

OA::SymHandle 
banal::OAInterface::getProcSymHandle(OA::ProcHandle h)
{ 
  Proc* p = IRHNDL_TO_TY(h, Proc*);
  DIAG_Assert(p == m_proc, "");

  const string& name = p->name(); // this gives us persistent data!
  const char* nm = name.c_str();
  return TY_TO_IRHNDL(nm, OA::SymHandle);
}

//***************************************************************************
//
//***************************************************************************

