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
//    IA64ISA.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_IA64ISA_hpp
#define isa_IA64ISA_hpp

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>
#include <include/gnu_bfd.h>
#include <include/gnu_dis-asm.h> // gnu binutils

#include "ISA.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// IA64ISA
//***************************************************************************

// 'IA64ISA': Implements the IA-64 Instruction Set Architecture.

class IA64ISA : public ISA {
public:
  IA64ISA();

  virtual ~IA64ISA();

  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------

  virtual ushort
  getInsnSize(MachInsn* GCC_ATTR_UNUSED mi)
  { return 16; /* Each IA64 VLIW packet is 16 bytes long. */ }

  virtual ushort
  getInsnNumOps(MachInsn *mi);

  virtual InsnDesc
  getInsnDesc(MachInsn* mi, ushort opIndex, ushort sz = 0);

  virtual VMA
  getInsnTargetVMA(MachInsn* mi, VMA pc, ushort opIndex, ushort sz = 0);

  virtual ushort
  getInsnNumDelaySlots(MachInsn* GCC_ATTR_UNUSED mi,
		       ushort GCC_ATTR_UNUSED opIndex,
		       ushort GCC_ATTR_UNUSED sz = 0)
  { return 0; }

  virtual bool
  isParallelWithSuccessor(MachInsn* GCC_ATTR_UNUSED mi1,
			  ushort GCC_ATTR_UNUSED opIndex1,
			  ushort GCC_ATTR_UNUSED sz1,
			  MachInsn* GCC_ATTR_UNUSED mi2,
			  ushort GCC_ATTR_UNUSED opIndex2,
			  ushort GCC_ATTR_UNUSED sz2) const
  { return false; /* FIXME */ }

  virtual VMA
  convertVMAToOpVMA(VMA vma, ushort opIndex) const
  {
    // This is identical to the GNU scheme for now.  Note that the
    // offsets do not actually match the IA64 template [5,41,41,41].
    //BriefAssertion(opIndex <= 2 && "Programming Error");
    return (vma + 6 * opIndex); // 0, 6, 12
  }

  virtual VMA
  convertOpVMAToVMA(VMA opvma, ushort& opIndex) const
  {
    // See above comments
    ushort offset = (opvma & 0xf); // 0, 6, 12
    opIndex = (ushort)(offset / 6);
    return (opvma - offset);
  }

  virtual void
  decode(std::ostream& os, MachInsn* mi, VMA vma, ushort opIndex);

private:
  // Should not be used
  IA64ISA(const IA64ISA& GCC_ATTR_UNUSED x)
  { }
  
  IA64ISA& operator=(const IA64ISA& GCC_ATTR_UNUSED x)
  { return *this; }

protected:
private:
  struct disassemble_info* m_di;
  struct disassemble_info* m_di_dis;
  GNUbu_disdata m_dis_data;
};

//****************************************************************************

#endif /* isa_IA64ISA_hpp */
