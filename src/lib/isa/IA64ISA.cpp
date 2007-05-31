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
//    IA64ISA.C
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
using std::ostream;

#ifdef NO_STD_CHEADERS
# include <stdarg.h>
# include <string.h>
#else
# include <cstdarg>
# include <cstring> // for 'memcpy'
using namespace std; // For compatibility with non-std C headers
#endif

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


//****************************************************************************
// IA64ISA
//****************************************************************************

IA64ISA::IA64ISA()
{
  // See 'dis-asm.h'
  di = new disassemble_info;
  init_disassemble_info(di, stdout, fake_fprintf_func);
  //  fprintf_func = (int (*)(void *, const char *, ...))fprintf;

  di->arch = bfd_arch_ia64;                //  bfd_get_arch (abfd);
  di->endian = BFD_ENDIAN_LITTLE;
  di->read_memory_func = read_memory_func; // vs. 'buffer_read_memory'
  di->print_address_func = print_addr;     // vs. 'generic_print_address'
}


ISA::InsnDesc
IA64ISA::GetInsnDesc(MachInsn* mi, ushort opIndex, ushort sz)
{
  MachInsn* gnuMI = ConvertMIToOpMI(mi, opIndex);
  InsnDesc d;

  if (CacheLookup(gnuMI) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(gnuMI), di);
    CacheSet(gnuMI, size);
  }

  switch(di->insn_type) {
    case dis_noninsn:
      d.Set(InsnDesc::INVALID);
      break;
    case dis_branch:
      if (di->target != 0) {
        d.Set(InsnDesc::BR_UN_COND_REL);
      }
      else {
        d.Set(InsnDesc::BR_UN_COND_IND);
      }
      break;
    case dis_condbranch:
      // N.B.: On the Itanium it is possible to have a one-bundle loop
      // (where the third slot branches to the first slot)!
      if (di->target != 0 || opIndex != 0) {
        d.Set(InsnDesc::INT_BR_COND_REL); // arbitrarily choose int
      }
      else {
        d.Set(InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;
    case dis_jsr:
      if (di->target != 0) {
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
IA64ISA::GetInsnTargetVMA(MachInsn* mi, VMA pc, ushort opIndex, ushort sz)
{
  MachInsn* gnuMI = ConvertMIToOpMI(mi, opIndex);

  if (CacheLookup(gnuMI) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(gnuMI), di);
    CacheSet(gnuMI, size);
  }
  
  // The target field is only set on instructions with targets.
  // N.B.: On the Itanium it is possible to have a one-bundle loop
  // (where the third slot branches to the first slot)!
  if (di->target != 0 || opIndex != 0)  {
    return (bfd_vma)di->target + (bfd_vma)pc;
  }
  else {
    return 0;
  }
}


ushort
IA64ISA::GetInsnNumOps(MachInsn* mi)
{
  // Because of the MLX template and data, we can't just return 3 here.
  if (CacheLookup(mi) == NULL) {
    int size = print_insn_ia64(PTR_TO_BFDVMA(mi), di);
    CacheSet(mi, size);
  }

  return (ushort)(di->target2);
}


void
IA64ISA::decode(MachInsn* mi, ostream& os)
{
  di->fprintf_func = dis_fprintf_func;
  di->stream = (void*)&os;
  print_insn_ia64(PTR_TO_BFDVMA(mi), di);
}
