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

#include "x86ISA.hpp"

#include <lib/support/diagnostics.h>

//*************************** macros ***************************



//*************************** Forward Declarations ***************************
void
GNU_print_addr(bfd_vma di_vma, struct disassemble_info* di)
{
  GNUbu_disdata* data = (GNUbu_disdata*)di->application_data;

  VMA x = GNUvma2vma(di_vma, data->insn_addr, data->insn_vma);
  ostream* os = (ostream*)di->stream;
  *os << std::showbase << std::hex << x << std::dec;
}


VMA
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



//****************************************************************************
