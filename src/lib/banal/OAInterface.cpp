// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
//   $HeadURL$
//
// Purpose:
//   A derivation of the IR interface for the ISA (disassembler) class
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

#include <include/gcc-attr.h>

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
BAnal::OAInterface::OAInterface (Proc* proc)
  : m_proc(proc)
{
  m_branchTargetSet.clear();
  for (ProcInsnIterator pii(*m_proc); pii.isValid(); ++pii) {
    Insn* insn = pii.current();
    //VMA curr_oppc = pii.currentVMA(); // the 'operation VMA'
    
    // If this insn is a branch, record its target address in
    // the branch target set.
    ISA::InsnDesc d = insn->desc();
    if (d.isBrRel()) {
      m_branchTargetSet.insert(normalizeTarget(insn->targetVMA(insn->vma())));
    }
  }
}


BAnal::OAInterface::~OAInterface()
{
  m_branchTargetSet.clear();
}


//-------------------------------------------------------------------------
// IRHandlesIRInterface
//-------------------------------------------------------------------------

string
BAnal::OAInterface::toString(const OA::ProcHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::CallHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::StmtHandle h)
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
BAnal::OAInterface::toString(const OA::ExprHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::OpHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::MemRefHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::SymHandle h)
{
  const char* sym = IRHNDL_TO_TY(h, const char*);
  string s(sym);
  return s;
}


string
BAnal::OAInterface::toString(const OA::ConstSymHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


string
BAnal::OAInterface::toString(const OA::ConstValHandle h)
{
  std::ostringstream oss;
  oss << h.hval();
  return oss.str();
}


void
BAnal::OAInterface::dump(OA::StmtHandle stmt, std::ostream& os)
{
  Insn* insn = IRHNDL_TO_TY(stmt, Insn*);
 
  // Currently, we just print the instruction type.  What we really
  // want is a textual disassembly of the instruction from the
  // disassembler.

  VMA pc = insn->vma();
  ISA::InsnDesc d = insn->desc();

  // Output pc and descriptor
  os << showbase << hex << pc << dec << ": " << d.toString();
  
  // Output other qualifiers
  if (m_branchTargetSet.find(pc) != m_branchTargetSet.end()) {
    os << " [branch target]";
  }
  if (d.isBrRel()) {
    VMA targ = insn->targetVMA(pc);
    os << " <" << hex << targ << dec << ">";
    if (m_proc->isIn(targ) == false) {
      os << " [out of procedure -- treat as RETURN via tail call]";
    }
  }
  else if (d.isBrInd()) {
    os << " <register>";
  }
}


void
BAnal::OAInterface::dump(OA::MemRefHandle GCC_ATTR_UNUSED h,
			 std::ostream& GCC_ATTR_UNUSED os)
{
  DIAG_Die(DIAG_Unimplemented);
}


void
BAnal::OAInterface::currentProc(OA::ProcHandle GCC_ATTR_UNUSED p)
{
  DIAG_Die(DIAG_Unimplemented);
}


//---------------------------------------------------------------------------
// CFGIRInterfaceDefault
//---------------------------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::procBody(OA::ProcHandle h)
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
BAnal::OAInterface::getCFGStmtType(OA::StmtHandle h)
{
  OA::CFG::IRStmtType ty;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  VMA br_targ = 0;

  ISA::InsnDesc d = insn->desc();
  if (d.isBrUnCondRel()) {
    // Unconditional jump. If the branch targets a PC outside of its
    // procedure, then we just ignore it.  For hpcstruct this is fine
    // since the branch won't create any loops.
    br_targ = insn->targetVMA(insn->vma());
    if (m_proc->isIn(br_targ)) {
      ty = OA::CFG::UNCONDITIONAL_JUMP;
    }
    else {
#if 0 
      ty = OA::CFG::SIMPLE;
#else
      // treat a branch out of the procedure (typically a tail call) as a return
      // 04 05 2014 - johnmc
      ty = OA::CFG::RETURN;
#endif
    }
  }
  else if (d.isBrUnCondInd()) {
    // Unconditional jump (indirect).
    //ty = OA::CFG::UNCONDITIONAL_JUMP_I; // FIXME: Turbo hack, temporary.
    ty = OA::CFG::SIMPLE;
  }
  else if (d.isBrCondRel()) {
    // Unstructured two-way branches. If the branch targets a PC
    // outside of its procedure, then we just ignore it.  For hpcstruct
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
  else if (d.isBrCondInd()) {
    ty = OA::CFG::SIMPLE; // FIXME: Turbo hack, temporary.
  }
  else if (d.isSubrRet()) {
    // Return statement.
    ty = OA::CFG::RETURN;
  }
  else if (d.isSubrRel()) {
    VMA callee = insn->targetVMA(insn->vma());
    if (m_proc->lm()->functionNeverReturns(callee)) {
      ty = OA::CFG::RETURN;
    } else {
      ty = OA::CFG::SIMPLE;
    }
  }
  else {
    // Simple statements.
    ty = OA::CFG::SIMPLE;
  }

  return ty;
}


OA::StmtLabel
BAnal::OAInterface::getLabel(OA::StmtHandle h)
{
  OA::StmtLabel lbl = 0;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  if (m_branchTargetSet.find(insn->vma()) != m_branchTargetSet.end()) {
    lbl = insn->vma();
  }
  return lbl;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::getFirstInCompound(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


//--------------------------------------------------------
// Loops
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::loopBody(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<OA::IRRegionStmtIterator>();
}


OA::StmtHandle
BAnal::OAInterface::loopHeader(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtHandle(0);
}


OA::StmtHandle
BAnal::OAInterface::getLoopIncrement(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtHandle(0);
}


bool
BAnal::OAInterface::loopIterationsDefinedAtEntry(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  return false;
}


//--------------------------------------------------------
// Structured two-way conditionals
//--------------------------------------------------------

OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::trueBody(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::elseBody(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


//--------------------------------------------------------
// Structured multiway conditionals
//--------------------------------------------------------

int
BAnal::OAInterface::numMultiCases(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return -1;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::multiBody(OA::StmtHandle GCC_ATTR_UNUSED h,
			      int GCC_ATTR_UNUSED bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


bool
BAnal::OAInterface::isBreakImplied(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return false;
}


bool
BAnal::OAInterface::isCatchAll(OA::StmtHandle GCC_ATTR_UNUSED h,
			       int GCC_ATTR_UNUSED bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return false;
}


OA::OA_ptr<OA::IRRegionStmtIterator>
BAnal::OAInterface::getMultiCatchall(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::OA_ptr<RegionStmtIterator>();
}


OA::ExprHandle
BAnal::OAInterface::getSMultiCondition(OA::StmtHandle GCC_ATTR_UNUSED h,
				       int GCC_ATTR_UNUSED bodyIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Unstructured two-way conditionals
//--------------------------------------------------------

OA::StmtLabel
BAnal::OAInterface::getTargetLabel(OA::StmtHandle h, int GCC_ATTR_UNUSED n)
{
  OA::StmtLabel lbl = 0;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  ISA::InsnDesc d = insn->desc();
  if (d.isBrRel()) {
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
BAnal::OAInterface::numUMultiTargets(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return -1;
}


OA::StmtLabel
BAnal::OAInterface::getUMultiTargetLabel(OA::StmtHandle GCC_ATTR_UNUSED h,
					 int GCC_ATTR_UNUSED targetIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtLabel(0);
}


OA::StmtLabel
BAnal::OAInterface::getUMultiCatchallLabel(OA::StmtHandle GCC_ATTR_UNUSED h)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::StmtLabel(0);
}


OA::ExprHandle
BAnal::OAInterface::getUMultiCondition(OA::StmtHandle GCC_ATTR_UNUSED h,
				       int GCC_ATTR_UNUSED targetIndex)
{
  DIAG_Die(DIAG_Unimplemented);
  return OA::ExprHandle(0);
}


//--------------------------------------------------------
// Special
//--------------------------------------------------------

int
BAnal::OAInterface::numberOfDelaySlots(OA::StmtHandle h)
{
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  return insn->numDelaySlots();
}


//--------------------------------------------------------
// Symbol Handles
//--------------------------------------------------------

OA::SymHandle
BAnal::OAInterface::getProcSymHandle(OA::ProcHandle h)
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

