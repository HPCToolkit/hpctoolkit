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
// Copyright ((c)) 2002-2022, Rice University
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
//    ISATypes.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_ISATypes_hpp
#define isa_ISATypes_hpp

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/uint.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

// Architectural datatypes:

// A virtual memory address for an arbitrary target machine.
typedef uint64_t VMA;
typedef int64_t VMASigned; // useful for offsets

#define VMA_MAX UINT64_MAX

// MachInsn* can point to (non-)variable length instructions (or
// instruction words) and should not be dereferenced.  To examine the
// individual bytes of a MachInsn*, use a 'MachInsnByte'.
typedef void MachInsn;
typedef unsigned char MachInsnByte;

// Casting helpers, in case we aren't on a 64-bit machine.
#define PTR_TO_BFDVMA(x)         ((uint64_t)(uintptr_t)(x))
#define BFDVMA_TO_PTR(x, totype) ((totype)(uintptr_t)(x))

#define PTR_TO_VMA(x)          PTR_TO_BFDVMA(x)
#define VMA_TO_PTR(x, totype)  BFDVMA_TO_PTR(x, totype)


//****************************************************************************

#if 0
struct lt_VMA {
  // return true if s1 < s2; false otherwise
  bool operator()(const VMA x, const VMA y) const { return x < y; }
};
#endif

#endif /* isa_ISATypes_hpp */
