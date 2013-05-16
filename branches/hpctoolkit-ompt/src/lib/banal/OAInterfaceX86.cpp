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
// Copyright ((c)) 2002-2013, Rice University
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

#include "OAInterfaceX86.hpp"

#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>

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
// OAInterfaceX86
//-----------------------------------------------------------------------------
using namespace BinUtil;

// OAInterfaceX86: We assume each instantiation of the IRInterface
// represents one procedure.  We then need to find all the possible
// branch targets.
BAnal::OAInterfaceX86::OAInterfaceX86 (Proc* proc)
  : OAInterface(proc)
{
  m_branchTargetSet.clear();
  for (ProcInsnIterator pii(*m_proc); pii.isValid(); ++pii) {
    Insn* insn = pii.current();
    
    // If this insn is a branch, record its target address in
    // the branch target set.
    ISA::InsnDesc d = insn->desc();
    if (d.isBrRel()) {
      m_branchTargetSet.insert(normalizeTarget(insn->targetVMA(insn->vma())));
    }
  }
}



//-------------------------------------------------------------------------
// IRHandlesIRInterface
//-------------------------------------------------------------------------

//---------------------------------------------------------------------------
// CFGIRInterfaceDefault
//---------------------------------------------------------------------------


//----------------------------------------------------------
// Statements: General
//----------------------------------------------------------

// Translate a ISA statement type into a IRStmtType.
OA::CFG::IRStmtType
BAnal::OAInterfaceX86::getCFGStmtType(OA::StmtHandle h)
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
      ty = OA::CFG::SIMPLE;
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
  else {
    // Simple statements.
    ty = OA::CFG::SIMPLE;
  }

  return ty;
}


OA::StmtLabel
BAnal::OAInterfaceX86::getLabel(OA::StmtHandle h)
{
  OA::StmtLabel lbl = 0;
  Insn* insn = IRHNDL_TO_TY(h, Insn*);
  if (m_branchTargetSet.find(insn->vma()) != m_branchTargetSet.end()) {
    lbl = insn->vma();
  }
  return lbl;
}



//--------------------------------------------------------
// Loops
//--------------------------------------------------------

//--------------------------------------------------------
// Structured two-way conditionals
//--------------------------------------------------------

//--------------------------------------------------------
// Structured multiway conditionals
//--------------------------------------------------------

//--------------------------------------------------------
// Unstructured two-way conditionals
//--------------------------------------------------------

OA::StmtLabel
BAnal::OAInterfaceX86::getTargetLabel(OA::StmtHandle h, int GCC_ATTR_UNUSED n)
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


//--------------------------------------------------------
// Special
//--------------------------------------------------------


//--------------------------------------------------------
// Symbol Handles
//--------------------------------------------------------

OA::SymHandle
BAnal::OAInterfaceX86::getProcSymHandle(OA::ProcHandle h)
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

