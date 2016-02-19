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

#include "PowerISA.hpp"

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
// PowerISA
//****************************************************************************

// Summary of interesting instructions:
//
//   [Relevant formats]
//     BD-form: conditional branches
//     I-form: unconditional branches
//     XL-form: indirect branches, among other things
//
// unconditional branch:
//   - relative [I]  {b:  branch}
//   - absolute [I]  {ba: branch, absolute}
//   - indirect      {bctr ~[bcctr]: branch to count reg}
//
// conditional branch:
//   - relative [BD] {bc    = b__:  branch cond}
//   - absolute [BD] {bca   = b__a: branch cond, absolute}
//   - indirect [XL] {bcctr = b_ctr: branch cond to count reg}
//
// subroutine call:
//   - relative [I]  {bl:  branch and link}
//   - absolute [I]  {bla: branch and link/absolute}
//   - indirect      {bctrl ~[bcctrl]: branch to count reg and link}
//
// subroutine call, conditional:
//   - relative [BD] {bcl = b__l:  branch cond and link}
//   - absolute [BD] {bcla = b__la: branch cond and link/absolute}
//   - indirect [XL] {bcctrl = b_ctrl = bccl: branch cond to count reg & link}
//
// return:
//   - uncond   [XL] {blr = br ~[bclr]: branch to link reg}
//   - cond     [XL] {bclr = b__lr = bcr: branch cond to link reg}
//   - CRAZY!   [XL] {blrl = brl ~[bclrl]: branch to link reg and link}
//   - CRAZY!   [XL] {bclrl = b__lrl = bcrl: branch cond to link reg and link}

PowerISA::PowerISA()
 : m_di(NULL), m_di_dis(NULL)
{
  // See 'dis-asm.h'
  m_di = new disassemble_info;
  init_disassemble_info(m_di, stdout, GNUbu_fprintf_stub);
  m_di->arch = bfd_arch_powerpc; // bfd_get_arch(abfd);
  m_di->mach = bfd_mach_ppc64;   // bfd_get_mach(abfd)
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


PowerISA::~PowerISA()
{
  delete m_di;
  delete m_di_dis;
}


ISA::InsnDesc
PowerISA::getInsnDesc(MachInsn* mi, ushort GCC_ATTR_UNUSED opIndex,
		      ushort GCC_ATTR_UNUSED sz)
{
  ISA::InsnDesc d;

  if (cacheLookup(mi) == NULL) {
    int size = print_insn_big_powerpc(PTR_TO_BFDVMA(mi), m_di);
    cacheSet(mi, (ushort)size);
  }

  // NOTE:
  //   m_di->target  is set for relative branch/subroutine targets
  //   m_di->target2 is set for absolute branch/subroutine targets
  bool isIndirect = (m_di->target == 0) && (m_di->target2 == 0);

  switch (m_di->insn_type) {
    case dis_noninsn:
      d.set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (isIndirect) {
	d.set(InsnDesc::BR_UN_COND_IND);
      }
      else {
	d.set(InsnDesc::BR_UN_COND_REL);
      }
      break;
    case dis_condbranch:
      if (isIndirect) {
	d.set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      else {
	d.set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (isIndirect) {
	d.set(InsnDesc::SUBR_IND);
      }
      else {
	d.set(InsnDesc::SUBR_REL);
      }
      break;
    case dis_condjsr:
      d.set(InsnDesc::OTHER);
      break;
    case dis_return:
      d.set(InsnDesc::SUBR_RET);
      break;
    case dis_dref:
    case dis_dref2:
      d.set(InsnDesc::MEM_OTHER);
      break;
    default:
      d.set(InsnDesc::OTHER);
      break;
  }
  return d;
}


VMA
PowerISA::getInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex, ushort sz)
{
  // N.B.: The GNU decoders assume that the address of 'mi' is
  // actually the VMA in order to calculate VMA-relative targets.

  if (cacheLookup(mi) == NULL) {
    int size = print_insn_big_powerpc(PTR_TO_BFDVMA(mi), m_di);
    cacheSet(mi, (ushort)size);
  }

  ISA::InsnDesc d = getInsnDesc(mi, opIndex, sz);
  if (d.isBrRel() || d.isSubrRel()) {
    // NOTE:
    //   m_di->target  is set to the displacement for relative targets
    //   m_di->target2 is set to the absolute value for absolute targets
    if (m_di->target != 0) {
      return vma + m_di->target;
    }
    else {
      return m_di->target2;
    }
  }
  else {
    return 0;
  }
}


ushort
PowerISA::getInsnNumDelaySlots(MachInsn* GCC_ATTR_UNUSED mi,
			       ushort GCC_ATTR_UNUSED opIndex,
			       ushort GCC_ATTR_UNUSED sz)
{
  // POWER does not have an architectural delay slot
  return 0;
}


void
PowerISA::decode(ostream& os, MachInsn* mi, VMA vma,
		 ushort GCC_ATTR_UNUSED opIndex)
{
  m_dis_data.insn_addr = mi;
  m_dis_data.insn_vma = vma;

  m_di_dis->stream = (void*)&os;
  print_insn_big_powerpc(PTR_TO_BFDVMA(mi), m_di_dis);
}


//****************************************************************************
