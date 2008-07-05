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
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <cstdarg>
#include <cstring> // for 'memcpy'

//*************************** User Include Files ****************************

#include <include/gnu_dis-asm.h>

#include "SparcISA.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

static VMA 
GNUvma2vma(bfd_vma di_vma, MachInsn* insn_addr, VMA insn_vma)
{ 
  // N.B.: The GNU decoders assume that the address of 'insn_addr' is
  // the actual the VMA in order to calculate VMA-relative targets.
  VMA x = (di_vma - PTR_TO_BFDVMA(insn_addr)) + insn_vma;
  return x;
}


static void 
GNUbu_print_addr(bfd_vma di_vma, struct disassemble_info* di)
{
  GNUbu_disdata* data = (GNUbu_disdata*)di->application_data;

  VMA x = GNUvma2vma(di_vma, data->insn_addr, data->insn_vma);
  ostream* os = (ostream*)di->stream;
  *os << std::showbase << std::hex << x << std::dec;
}


//****************************************************************************
// SparcISA
//****************************************************************************

SparcISA::SparcISA()
 : m_di(NULL), m_di_dis(NULL)
{
  // See 'dis-asm.h'
  m_di = new disassemble_info;
  init_disassemble_info(m_di, stdout, GNUbu_fprintf_stub);
  m_di->arch = bfd_arch_sparc;    // bfd_get_arch (abfd);
  m_di->mach = bfd_mach_sparc_v9; // bfd_get_mach(abfd)
  m_di->endian = BFD_ENDIAN_BIG;
  m_di->read_memory_func = GNUbu_read_memory; // vs. 'buffer_read_memory'
  m_di->print_address_func = GNUbu_print_addr_stub; // vs. 'generic_print_addr'

  m_di_dis = new disassemble_info;
  init_disassemble_info(m_di_dis, stdout, GNUbu_fprintf);
  m_di_dis->application_data = (void*)&m_dis_data;
  m_di_dis->arch = m_di->arch;
  m_di_dis->mach = m_di->mach;
  m_di_dis->endian = m_di->endian;
  m_di_dis->read_memory_func = GNUbu_read_memory;
  m_di_dis->print_address_func = GNUbu_print_addr;
}


SparcISA::~SparcISA()
{
  delete m_di;
  delete m_di_dis;
}


ISA::InsnDesc
SparcISA::GetInsnDesc(MachInsn* mi, ushort opIndex, ushort sz)
{
  ISA::InsnDesc d;

  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_sparc(PTR_TO_BFDVMA(mi), m_di);
    CacheSet(mi, size);
  }

  // The target field is set (to an absolute vma) on PC-relative
  // branches/jumps.  However, the target field is also set after
  // sequences of the form {sethi, insn}, where insn uses the same
  // register as in sethi.  Because {sethi, jmp} sequences are
  // possible, the target field can also be set (to an absolute vma)
  // in the case of a register indirect jump.  We need some way to
  // distinguish because of the way GNU calculates PC-relative
  // targets.
  bool isPCRel = (m_di->target2 == 0); // FIXME: binutils needs fixing

  switch (m_di->insn_type) {
    case dis_noninsn:
      d.Set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (m_di->target != 0 && isPCRel) {
	d.Set(InsnDesc::BR_UN_COND_REL);
      }
      else {
	d.Set(InsnDesc::BR_UN_COND_IND);
      }
      break;
    case dis_condbranch:
      if (m_di->target != 0 && isPCRel) {
	d.Set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      }
      else {
	d.Set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (m_di->target != 0 && isPCRel) {
	d.Set(InsnDesc::SUBR_REL);
      }
      else {
	d.Set(InsnDesc::SUBR_IND);
      }
      break;
    case dis_condjsr:
      d.Set(InsnDesc::OTHER);
      break;
    case dis_return:
      d.Set(InsnDesc::SUBR_RET);
      break;
    case dis_dref:
    case dis_dref2:
      d.Set(InsnDesc::MEM_OTHER);
      break;
    default:
      d.Set(InsnDesc::OTHER);
      break;
  }
  return d;
}


VMA
SparcISA::GetInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex, ushort sz)
{
  // N.B.: The GNU decoders assume that the address of 'mi' is
  // actually the VMA in order to calculate VMA-relative targets.

  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_sparc(PTR_TO_BFDVMA(mi), m_di);
    CacheSet(mi, size);
  }
  
  ISA::InsnDesc d = GetInsnDesc(mi, opIndex, sz);
  if (d.IsBrRel() || d.IsSubrRel()) {
    return GNUvma2vma(m_di->target, mi, vma);
  }
  else {
    // return m_di->target; // return the results of sethi instruction chains
    return 0;
  }
}


ushort
SparcISA::GetInsnNumDelaySlots(MachInsn* mi, ushort opIndex, ushort sz)
{ 
  // SPARC branch instructions have an architectural delay slot of one
  // instruction, but in certain cases it can effectively be zero.  If
  // the annul bit is set in the case of an unconditional
  // (branch-always or branch-never) branch instruction (that is, BA,
  // FBA, CBA, BN, FBN, and CBN), the delay instruction is not
  // executed.

  // Unfortunately there is not yet a good way to pass annullment
  // information back from binutils.  Fortunately, it is currently not
  // that important.

  // (We don't care about delays on instructions such as loads/stores)

  ISA::InsnDesc d = GetInsnDesc(mi, opIndex, sz);
  if (d.IsBr() || d.IsSubr() || d.IsSubrRet()) {
    return 1;
  }
  else {
    return 0;
  }
}


void
SparcISA::decode(ostream& os, MachInsn* mi, VMA vma, ushort opIndex)
{
  m_dis_data.insn_addr = mi;
  m_dis_data.insn_vma = vma;

  m_di_dis->stream = (void*)&os;
  print_insn_sparc(PTR_TO_BFDVMA(mi), m_di_dis);
}


//****************************************************************************
