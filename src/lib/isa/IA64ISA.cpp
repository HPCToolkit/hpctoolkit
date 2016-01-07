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

#include "IA64ISA.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

static MachInsn*
ConvertMIToOpMI(MachInsn* mi, ushort opIndex)
{
  // Do not change; the GNU decoders depend upon these particular
  // offsets.  Note that the offsets do not actually match the IA64
  // template [5,41,41,41].
  DIAG_Assert(opIndex <= 2, "Programming Error");
  return (MachInsn*)((MachInsnByte*)mi + (6 * opIndex)); // 0, 6, 12
}


static VMA
GNUvma2vma(bfd_vma di_vma, MachInsn* GCC_ATTR_UNUSED insn_addr, VMA insn_vma)
{
  VMA x = (VMA)di_vma + insn_vma;
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
// IA64ISA
//****************************************************************************

IA64ISA::IA64ISA()
  : m_di(NULL), m_di_dis(NULL)
{
  // See 'dis-asm.h'
  m_di = new disassemble_info;
  init_disassemble_info(m_di, stdout, GNUbu_fprintf_stub);
  m_di->arch = bfd_arch_ia64;                //  bfd_get_arch (abfd);
  m_di->endian = BFD_ENDIAN_LITTLE;
  m_di->read_memory_func = GNUbu_read_memory; // vs. 'buffer_read_memory'
  m_di->print_address_func = GNUbu_print_addr_stub; // vs. 'generic_print_addr'

  m_di_dis = new disassemble_info;
  init_disassemble_info(m_di_dis, stdout, GNUbu_fprintf);
  m_di_dis->application_data = (void*)&m_dis_data;
  m_di_dis->arch = m_di->arch;
  m_di_dis->endian = m_di->endian;
  m_di_dis->read_memory_func = GNUbu_read_memory;
  m_di_dis->print_address_func = GNUbu_print_addr;
}


IA64ISA::~IA64ISA()
{
  delete m_di;
  delete m_di_dis;
}


ISA::InsnDesc
IA64ISA::getInsnDesc(MachInsn* mi, ushort opIndex, ushort GCC_ATTR_UNUSED sz)
{
  MachInsn* gnuMI = ConvertMIToOpMI(mi, opIndex);
  InsnDesc d;

  if (cacheLookup(gnuMI) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(gnuMI), m_di);
    cacheSet(gnuMI, (ushort)size);
  }

  switch(m_di->insn_type) {
    case dis_noninsn:
      d.set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (m_di->target != 0) {
        d.set(InsnDesc::BR_UN_COND_REL);
      }
      else {
        d.set(InsnDesc::BR_UN_COND_IND);
      }
      break;
    case dis_condbranch:
      // N.B.: On the Itanium it is possible to have a one-bundle loop
      // (where the third slot branches to the first slot)!
      if (m_di->target != 0 || opIndex != 0) {
        d.set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      }
      else {
        d.set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (m_di->target != 0) {
        d.set(InsnDesc::SUBR_REL);
      }
      else {
        d.set(InsnDesc::SUBR_IND);
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
IA64ISA::getInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex,
			  ushort GCC_ATTR_UNUSED sz)
{
  MachInsn* gnuMI = ConvertMIToOpMI(mi, opIndex);

  if (cacheLookup(gnuMI) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(gnuMI), m_di);
    cacheSet(gnuMI, (ushort)size);
  }

  // The target field is only set on instructions with targets.
  // N.B.: On the Itanium it is possible to have a one-bundle loop
  // (where the third slot branches to the first slot)!
  if (m_di->target != 0 || opIndex != 0)  {
    return GNUvma2vma(m_di->target, mi, vma);
  }
  else {
    return 0;
  }
}


ushort
IA64ISA::getInsnNumOps(MachInsn* mi)
{
  // Because of the MLX template and data, we can't just return 3 here.
  if (cacheLookup(mi) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(mi), m_di);
    cacheSet(mi, (ushort)size);
  }

  return (ushort)(m_di->target2);
}


void
IA64ISA::decode(ostream& os, MachInsn* mi, VMA vma,
		ushort GCC_ATTR_UNUSED opIndex)
{
  m_dis_data.insn_addr = mi;
  m_dis_data.insn_vma = vma;

  m_di_dis->stream = (void*)&os;
  print_insn_ia64(PTR_TO_BFDVMA(mi), m_di_dis);
}
