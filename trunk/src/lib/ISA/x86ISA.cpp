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
//    i686ISA.C
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

#include "i686ISA.h"
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
  memcpy(myaddr, (const char *)vma, len);
  return 0; /* success */
}

} // extern "C"

//****************************************************************************
// i686ISA
//****************************************************************************

i686ISA::i686ISA()
{
  // See 'dis-asm.h'
  di = new disassemble_info;
  INIT_DISASSEMBLE_INFO(*di, stdout, fake_fprintf_func);
  //  fprintf_func: (int (*)(void *, const char *, ...))fprintf;

  di->arch = bfd_arch_i386;                //  bfd_get_arch (abfd);
  di->endian = BFD_ENDIAN_LITTLE;
  di->read_memory_func = read_memory_func; // vs. 'buffer_read_memory'
  di->print_address_func = print_addr;     // vs. 'generic_print_address'
}

i686ISA::~i686ISA()
{
  delete di;
}

ushort
i686ISA::GetInstSize(MachInst* mi)
{
  ushort size;
  DecodingCache *cache;

  if ((cache = CacheLookup(mi)) == NULL) {
    size = print_insn_i386((bfd_vma)mi, di);
    CacheSet(mi, size);
  } else {
    size = cache->instSize;
  }
  return size;
}

ISA::InstType
i686ISA::GetInstType(MachInst* mi, ushort opIndex, ushort s)
{
  ISA::InstType t;

  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386((bfd_vma)mi, di);
    CacheSet(mi, size);
  }

  switch(di->insn_type) {
    case dis_noninsn:
      t = ISA::INVALID;
      break;
    case dis_branch:
      if (di->target != 0) {
	t = ISA::BR_UN_COND_REL;
      } else {
	t = ISA::BR_UN_COND_IND;
      }
      break;
    case dis_condbranch:
      if (di->target != 0) {
	t = ISA::BR_COND_REL;
      } else {
	t = ISA::BR_COND_IND;
      }
      break;
    case dis_jsr:
      if (di->target != 0) {
	t = ISA::SUBR_REL;
      } else {
	t = ISA::SUBR_IND;
      }
      break;
    case dis_condjsr:
      t = ISA::OTHER;
    case dis_dref:
    case dis_dref2:
      t = ISA::MEM; 
    default:
      t = ISA::OTHER;
      break;
  }
  return t;
}

Addr
i686ISA::GetInstTargetAddr(MachInst* mi, Addr pc, ushort opIndex, ushort sz)
{
  if (CacheLookup(mi) == NULL) {
    ushort size = print_insn_i386((bfd_vma)mi, di);
    CacheSet(mi, size);
  }

  // N.B.: The GNU decoders expect that the address of the 'mi' is
  // actually the PC/vma.  Furthermore, because i686 is 32-bit
  // decoding, it only keeps 32 bits of 'mi'.

  // The target field is only set on instructions with targets.
  if (di->target != 0) {
    return (di->target - ((bfd_vma)(mi) & 0xffffffff)) + (bfd_vma)pc;
  } else {
    return 0;
  }
}


