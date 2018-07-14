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
// Copyright ((c)) 2002-2018, Rice University
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

// This file defines empty methods for the virtual functions in the
// ISA class.  This allows creating an instance of the ISA class that
// we don't actually use, but where a NULL object would cause other
// things to fail.
//
// For example, this is useful for the new platforms (power8/le and
// ARM) that have no ISA extension for their architecture.  For those
// platforms and whenever using the ParseAPI CFG support, binutils's
// LM class creates an ISA object but doesn't use it.
//
// The alternative would be to walk through the code and add #ifdef
// and tests for NULL as needed.  But this is probably simpler.

//***************************************************************************

#ifndef isa_EmptyISA_hpp
#define isa_EmptyISA_hpp

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "ISA.hpp"

class EmptyISA : public ISA {
public:
  EmptyISA() { }

  virtual ~EmptyISA() { }

  virtual ushort
  getInsnSize(MachInsn* GCC_ATTR_UNUSED mi)
  { return 4; }

  virtual ushort
  getInsnNumOps(MachInsn* GCC_ATTR_UNUSED mi)
  { return 1; }

  virtual InsnDesc
  getInsnDesc(MachInsn* mi, ushort opIndex, ushort sz = 0)
  {
    ISA::InsnDesc d;
    return d;
  }

  virtual VMA
  getInsnTargetVMA(MachInsn* mi, VMA pc, ushort opIndex, ushort sz = 0)
  { return 0; }

  virtual ushort
  getInsnNumDelaySlots(MachInsn* mi, ushort opIndex, ushort sz = 0)
  { return 0; }

  virtual bool
  isParallelWithSuccessor(MachInsn* GCC_ATTR_UNUSED mi1,
                          ushort GCC_ATTR_UNUSED opIndex1,
                          ushort GCC_ATTR_UNUSED sz1,
                          MachInsn* GCC_ATTR_UNUSED mi2,
                          ushort GCC_ATTR_UNUSED opIndex2,
                          ushort GCC_ATTR_UNUSED sz2) const
  { return false; }

  virtual void
  decode(std::ostream& os, MachInsn* mi, VMA vma, ushort opIndex) { }
};

#endif  // isa_EmptyISA_hpp
