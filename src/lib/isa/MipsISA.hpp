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
//    MipsISA.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_MipsISA_hpp
#define isa_MipsISA_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>
#include <include/gnu_bfd.h>

#include "ISA.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// MipsISA
//***************************************************************************

// 'MipsISA': Implements the MIPS Instruction Set Architecture.
// See 'ISA.h' for comments on the interface

class MipsISA : public ISA {
public:
  MipsISA();
  virtual ~MipsISA();

  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------

  virtual ushort
  getInsnSize(MachInsn* GCC_ATTR_UNUSED mi)
  { return MINSN_SIZE; }

  virtual ushort
  getInsnNumOps(MachInsn* GCC_ATTR_UNUSED mi)
  { return 1; }

  virtual InsnDesc
  getInsnDesc(MachInsn* mi, ushort opIndex, ushort sz = 0);

  virtual VMA
  getInsnTargetVMA(MachInsn* mi, VMA pc, ushort opIndex, ushort sz = 0);

  virtual ushort
  getInsnNumDelaySlots(MachInsn* mi, ushort opIndex, ushort sz = 0);

  virtual bool
  isParallelWithSuccessor(MachInsn* GCC_ATTR_UNUSED mi1,
			  ushort GCC_ATTR_UNUSED opIndex1,
			  ushort GCC_ATTR_UNUSED sz1,
			  MachInsn* GCC_ATTR_UNUSED mi2,
			  ushort GCC_ATTR_UNUSED opIndex2,
			  ushort GCC_ATTR_UNUSED sz2) const
  { return false; }

  virtual void
  decode(std::ostream& os, MachInsn* mi, VMA vma, ushort opIndex);

private:
  // Should not be used
  MipsISA(const MipsISA& GCC_ATTR_UNUSED x)
  { }

  MipsISA&
  operator=(const MipsISA& GCC_ATTR_UNUSED x)
  { return *this; }

protected:
private:
  static const ushort MINSN_SIZE = 4; // machine instruction size in bytes
  struct disassemble_info* m_di;
  struct disassemble_info* m_di_dis;
  GNUbu_disdata m_dis_data;
};

//****************************************************************************

#endif /* isa_MipsISA_hpp */
