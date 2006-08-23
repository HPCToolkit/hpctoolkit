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
//    x86ISA.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <stdarg.h>
# include <string.h>
#else
# include <cstdarg>
# include <cstring> // for 'memcpy'
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "x86ISA.hpp"
#include <include/gnu_dis-asm.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

extern "C" { 

static int fake_fprintf_func(PTR, const char *fmt, ...)
{
  //va_list ap;
  //va_start(ap, fmt);
  return 0;
}

static void print_addr (bfd_vma vma, struct disassemble_info *info)
{
  info->fprintf_func (info->stream, "0x%08x", vma);
}

static int read_memory_func (bfd_vma vma, bfd_byte *myaddr, unsigned int len,
			     struct disassemble_info *info)
{
  memcpy(myaddr, BFDVMA_TO_PTR(vma, const char*), len);
  return 0; /* success */
}

} // extern "C"

//****************************************************************************
// x86ISA
//****************************************************************************

x86ISA::x86ISA(bool is_x86_64)
  : mIs_x86_64(is_x86_64), di(NULL)
{
  // See 'dis-asm.h'
  di = new disassemble_info;
  INIT_DISASSEMBLE_INFO(*di, stdout, fake_fprintf_func);
  //  fprintf_func: (int (*)(void *, const char *, ...))fprintf;

  di->arch = bfd_arch_i386;      // bfd_get_arch(abfd);
  if (mIs_x86_64) {              // bfd_get_mach(abfd); needed in print_insn()
    di->mach = bfd_mach_x86_64;
  }
  else {
    di->mach = bfd_mach_i386_i386;
  }
  di->endian = BFD_ENDIAN_LITTLE;
  di->read_memory_func = read_memory_func; // vs. 'buffer_read_memory'
  di->print_address_func = print_addr;     // vs. 'generic_print_address'
}

x86ISA::~x86ISA()
{
  delete di;
}

ushort
x86ISA::GetInsnSize(MachInsn* mi)
{
  ushort size;
  DecodingCache *cache;

  if ((cache = CacheLookup(mi)) == NULL) {
    size = print_insn_i386(PTR_TO_BFDVMA(mi), di);
    CacheSet(mi, size);
  } else {
    size = cache->insnSize;
  }
  return size;
}

ISA::InsnDesc
x86ISA::GetInsnDesc(MachInsn* mi, ushort opIndex, ushort s)
{
  ISA::InsnDesc d;

  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386(PTR_TO_BFDVMA(mi), di);
    CacheSet(mi, size);
  }

  switch(di->insn_type) {
    case dis_noninsn:
      d.Set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (di->target != 0) {
	d.Set(InsnDesc::BR_UN_COND_REL);
      } else {
	d.Set(InsnDesc::BR_UN_COND_IND);
      }
      break;
    case dis_condbranch:
      if (di->target != 0) {
	d.Set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      } else {
	d.Set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (di->target != 0) {
	d.Set(InsnDesc::SUBR_REL);
      } else {
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
x86ISA::GetInsnTargetVMA(MachInsn* mi, VMA pc, ushort opIndex, ushort sz)
{
  static const bfd_vma M32 = 0xffffffff;
  
  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386(PTR_TO_BFDVMA(mi), di);
    CacheSet(mi, size);
  }

  // N.B.: The GNU decoders expect that the address of the 'mi' is
  // actually the PC/vma.  Furthermore for 32-bit x86 decoding, only
  // the lower 32 bits of 'mi' are valid.
  
  // The target field is only set on instructions with targets.
  if (di->target != 0) {
    //VMA t = (mIs_x86_64) ?
    //  ((di->target & M32) - (PTR_TO_BFDVMA(mi) & M32)) + (bfd_vma)pc :
    //  ((di->target)       - (PTR_TO_BFDVMA(mi) & M32)) + (bfd_vma)pc;
    VMA t = ((di->target & M32) - (PTR_TO_BFDVMA(mi) & M32)) + (bfd_vma)pc;
    return t;
  } 
  else {
    return 0;
  }
}


