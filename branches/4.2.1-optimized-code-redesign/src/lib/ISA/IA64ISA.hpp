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
//    IA64ISA.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef IA64ISA_H 
#define IA64ISA_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>
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
  virtual ~IA64ISA() { }
  
  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------  
  
  virtual ushort GetInstSize(MachInst* mi)
  { return 16; /* Each IA64 VLIW packet is 16 bytes long. */ }
  
  virtual ushort GetInstNumOps(MachInst *mi);

  virtual InstDesc GetInstDesc(MachInst* mi, ushort opIndex, ushort sz = 0);

  virtual VMA GetInstTargetVMA(MachInst* mi, VMA pc, ushort opIndex,
				 ushort sz = 0);
  
  virtual ushort GetInstNumDelaySlots(MachInst* mi, ushort opIndex,
				      ushort sz = 0)
  { return 0; }

  virtual bool IsParallelWithSuccessor(MachInst* mi1, ushort opIndex1,
				       ushort sz1,
				       MachInst* mi2, ushort opIndex2,
				       ushort sz2) const
  { return false; /* FIXME */ }

  virtual VMA ConvertVMAToOpVMA(VMA vma, ushort opIndex) const
  {
    // This is identical to the GNU scheme for now.  Note that the
    // offsets do not actually match the IA64 template [5,41,41,41].
    //BriefAssertion(opIndex <= 2 && "Programming Error");
    return (vma + 6 * opIndex); // 0, 6, 12
  }

  virtual VMA ConvertOpVMAToVMA(VMA opvma, ushort& opIndex) const
  {
    // See above comments
    ushort offset = (opvma & 0xf); // 0, 6, 12
    opIndex = offset / 6;
    return (opvma - offset);
  }

private: 
  // Should not be used
  IA64ISA(const IA64ISA& i) { }
  IA64ISA& operator=(const IA64ISA& i) { return *this; }

protected:
private:
  struct disassemble_info *di;
};

//****************************************************************************

#endif 
