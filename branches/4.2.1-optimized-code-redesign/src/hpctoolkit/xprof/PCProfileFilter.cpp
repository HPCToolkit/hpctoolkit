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
//    PCProfileFilter.C
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

//*************************** User Include Files ****************************

#include "PCProfileFilter.hpp"

#include <lib/binutils/Instruction.hpp>

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;


//****************************************************************************
// PCProfileFilter
//****************************************************************************

PCProfileFilter::~PCProfileFilter()
{
  delete mfilt;
  delete pcfilt;
}

void 
PCProfileFilter::Dump(std::ostream& o)
{
}

void 
PCProfileFilter::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************
// InsnClassExpr
//****************************************************************************

InsnClassExpr::bitvec_t 
ConvertToInsnClass(ISA::InstDesc d)
{
  if (d.IsFP()) { return INSN_CLASS_FLOP; }
  else if (d.IsMemOp()) { return INSN_CLASS_MEMOP; }
  else if (d.IsIntOp()) { return INSN_CLASS_INTOP; }
  else { return INSN_CLASS_OTHER; }
}

//****************************************************************************
// InsnFilter
//****************************************************************************

InsnFilter::InsnFilter(InsnClassExpr expr_, LoadModule* lm_)
  : expr(expr_), lm(lm_)
{
}

InsnFilter::~InsnFilter()
{
}

bool 
InsnFilter::operator()(VMA pc, ushort opIndex)
{
  Instruction* inst = lm->GetInst(pc, opIndex);
  if (!inst) {
    return false;
  }

  return (expr.IsSatisfied(ConvertToInsnClass(inst->GetDesc())));
}

//****************************************************************************
// PCProfileFilterList
//****************************************************************************

void 
PCProfileFilterList::destroyContents() 
{
  for (iterator it = begin(); it != end(); ++it) { delete (*it); }
  clear();
}
