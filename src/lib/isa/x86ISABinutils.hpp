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
//    x86ISABinutils.hpp
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_x86ISABinutils_hpp
#define isa_x86ISABinutils_hpp

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "x86ISA.hpp"

//*************************** Forward Declarations ***************************

struct disassemble_info;

//****************************************************************************

//***************************************************************************
// x86ISABinutils
//***************************************************************************

// 'x86ISABinutils': Implements the x86 and x86-64 Instruction Set Architecture
//			using GNU Binutils to crack the binary
// See comments in 'ISA.h'

class x86ISABinutils : public x86ISA {
public:
  x86ISABinutils(bool is_x86_64 = false);
  virtual ~x86ISABinutils();

  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------

  virtual ushort
  getInsnSize(MachInsn* mi);

  virtual InsnDesc
  getInsnDesc(MachInsn* mi, ushort opIndex, ushort sz = 0);

  virtual VMA
  getInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex, ushort sz = 0);

  virtual void
  decode(std::ostream& os, MachInsn* mi, VMA vma, ushort opIndex);


private:
  bool m_is_x86_64;
  struct disassemble_info* m_di;
  struct disassemble_info* m_di_dis;
  GNUbu_disdata m_dis_data;

};



//****************************************************************************

#endif /* isa_x86ISABinutils_hpp */
