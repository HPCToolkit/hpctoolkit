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
//    ISATypes.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ISATypes_H 
#define ISATypes_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>
#include <include/gnu_bfd.h> // for 'bfd_vma'

//*************************** Forward Declarations ***************************

//****************************************************************************

// Architectural datatypes: 

// A memory address for an arbitrary target machine.  Take advantage
// of 'bfd_vma' so we don't have to mess with differently sized address
// spaces.  (0 is the null value)
typedef bfd_vma Addr;
typedef bfd_signed_vma AddrSigned; // useful for offsets

// MachInst* can point to (non-)variable length instructions (or
// instruction words) and should not be dereferenced.  To examine the
// individual bytes of a MachInst*, use a 'MachInstByte'.
typedef void MachInst;
typedef unsigned char MachInstByte;

// When GNU binutils is built as a cross-platform tool, bfd_vma will
// be 64-bits on a 32-bit machine.  Use these casting macros to
// eliminate compiler warnings about, e.g., "casting a 32-bit pointer
// to an integer of different size".
#define PTR_TO_BFDVMA(x)         ((bfd_vma)(psuint)(x))
#define BFDVMA_TO_PTR(x, totype) ((totype)(psuint)(x))

#define PTR_TO_ADDR(x)          PTR_TO_BFDVMA(x)
#define ADDR_TO_PTR(x, totype)  BFDVMA_TO_PTR(x, totype)


//****************************************************************************

struct lt_Addr {
  // return true if s1 < s2; false otherwise
  bool operator()(const Addr a1, const Addr a2) const { return a1 < a2; }
};

#endif 
