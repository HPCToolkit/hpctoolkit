// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include "x86ISA.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

static VMA 
GNUvma2vma(bfd_vma di_vma, MachInsn* insn_addr, VMA insn_vma)
{ 
  // N.B.: The GNU decoders expect that the address of the 'mi' is
  // actually the VMA.  Furthermore for 32-bit x86 decoding, only
  // the lower 32 bits of 'mi' are valid.

  static const bfd_vma M32 = 0xffffffff;
  //VMA t = (m_is_x86_64) ?
  //  ((m_di->target & M32) - (PTR_TO_BFDVMA(mi) & M32)) + (bfd_vma)vma :
  //  ((m_di->target)       - (PTR_TO_BFDVMA(mi) & M32)) + (bfd_vma)vma;
  VMA x = ((di_vma & M32) - (PTR_TO_BFDVMA(insn_addr) & M32)) + (bfd_vma)insn_vma;
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
// x86ISA
//****************************************************************************

x86ISA::x86ISA(bool is_x86_64)
  : m_is_x86_64(is_x86_64), m_di(NULL), m_di_dis(NULL)
{
  // See 'dis-asm.h'
  m_di = new disassemble_info;
  init_disassemble_info(m_di, stdout, GNUbu_fprintf_stub);
  m_di->arch = bfd_arch_i386;     // bfd_get_arch(abfd);
  if (m_is_x86_64) {              // bfd_get_mach(abfd); needed in print_insn()
    m_di->mach = bfd_mach_x86_64;
  }
  else {
    m_di->mach = bfd_mach_i386_i386;
  }
  m_di->endian = BFD_ENDIAN_LITTLE;
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


x86ISA::~x86ISA()
{
  delete m_di;
  delete m_di_dis;
}


ushort
x86ISA::GetInsnSize(MachInsn* mi)
{
  ushort size;
  DecodingCache *cache;

  if ((cache = CacheLookup(mi)) == NULL) {
    size = print_insn_i386(PTR_TO_BFDVMA(mi), m_di);
    CacheSet(mi, size);
  }
  else {
    size = cache->insnSize;
  }
  return size;
}


ISA::InsnDesc
x86ISA::GetInsnDesc(MachInsn* mi, ushort opIndex, ushort s)
{
  ISA::InsnDesc d;

  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386(PTR_TO_BFDVMA(mi), m_di);
    CacheSet(mi, size);
  }

  switch(m_di->insn_type) {
    case dis_noninsn:
      d.Set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (m_di->target != 0) {
	d.Set(InsnDesc::BR_UN_COND_REL);
      }
      else {
	d.Set(InsnDesc::BR_UN_COND_IND);
      }
      break;
    case dis_condbranch:
      if (m_di->target != 0) {
	d.Set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      }
      else {
	d.Set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (m_di->target != 0) {
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
x86ISA::GetInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex, ushort sz)
{
  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386(PTR_TO_BFDVMA(mi), m_di);
    CacheSet(mi, size);
  }

  // The target field is only set on instructions with targets.
  if (m_di->target != 0) {
    return GNUvma2vma(m_di->target, mi, vma);
  } 
  else {
    return 0;
  }
}


void
x86ISA::decode(ostream& os, MachInsn* mi, VMA vma, ushort opIndex)
{
  m_dis_data.insn_addr = mi;
  m_dis_data.insn_vma = vma;
  
  m_di_dis->stream = (void*)&os;
  print_insn_i386(PTR_TO_BFDVMA(mi), m_di_dis);
}


//****************************************************************************
