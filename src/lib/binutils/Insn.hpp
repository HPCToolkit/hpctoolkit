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

#ifndef BinUtil_Insn_hpp
#define BinUtil_Insn_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "LM.hpp"

#include <lib/isa/ISA.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// Insn
//***************************************************************************

namespace BinUtil {

// 'Insn' is an abstract class that represents one single
// processor instruction instruction in the 'TextSection' of a
// 'LoadModule'.  For the sake of generality, all instructions are
// viewed as (potentially) variable sized instruction packets.
//
// VLIW instructions (or 'packets') that consist of multiple
// operations, are "unpacked" or "flattened" into their component
// operations; thus each operation is an 'Insn'.

class Insn {
public:
  Insn(MachInsn* minsn)
    : m_minsn(minsn), m_vma(0)
  { }

  Insn(MachInsn* minsn, VMA vma)
    : m_minsn(minsn), m_vma(vma)
  { }

  virtual
  ~Insn()
  { }

  // Returns a classification of the instruction
  ISA::InsnDesc desc() const
  { return LM::isa->getInsnDesc(m_minsn, opIndex(), size()); }

  // 'bits' returns a pointer to the bits of the instruction;
  // 'size' returns the size of the machine instruction.
  // In the case of VLIW instructions, returns the {bits, size} not of
  // the individual operation but the whole "packet".
  virtual MachInsn*
  bits() const
  { return m_minsn; }

  virtual ushort
  size() const = 0;

  // Returns the VMA for the beginning of this instruction.
  // WARNING: this is unrelocated
  VMA
  vma() const
  { return m_vma; }

  void
  vma(VMA vma)
  { m_vma = vma; }

  // Returns the end vma, i.e. the vma immediately following this instruction
  VMA
  endVMA() const
  { return vma() + size(); }

  // Returns the operation VMA for the beginning of this instruction
  VMA
  opVMA() const
  { return LM::isa->convertVMAToOpVMA(vma(), opIndex()); }
  
  // Viewing each object code instruction as an instruction packet,
  // and recalling that all packets are "unpacked" in a 'LM',
  // 'GetOpIndex' retuns the (0 based) index of this instruction in
  // the original packet.  'GetNumOps' returns the number of
  // operations in this packet.  For RISC and CISC machines these
  // values will be 0 and 1, respectively.
  virtual ushort
  opIndex() const = 0;

  virtual ushort
  numOps() const = 0;

  // Returns the target address of a jump or branch instruction. If a
  // target cannot be computed, return 0.  Note that a target is not
  // computed when it depends on values in registers (e.g. indirect
  // jumps).  'vma' is used only to calculate PC-relative targets.
  virtual VMA
  targetVMA(VMA vma) const
  { return LM::isa->getInsnTargetVMA(m_minsn, vma, opIndex(), size()); }
  
  // Returns the number of delay slots that must be observed by
  // schedulers before the effect of instruction 'mi' can be
  // assumed to be fully obtained (e.g., RISC braches).
  virtual ushort
  numDelaySlots() const
  { return LM::isa->getInsnNumDelaySlots(m_minsn, opIndex(), size()); }

  // Returns whether or not the instruction "explicitly" executes in
  // parallel with its successor 'mi2' (successor in the sequential
  // sense).  IOW, this has special reference to "explicitly parallel"
  // architecture, not superscalar design.
  virtual bool
  isParallelWithSuccessor(Insn* x) const
  {
    return LM::isa->isParallelWithSuccessor(m_minsn, opIndex(), size(),
					    x->bits(), x->opIndex(),
					    x->size());
  }

  // decode:
  virtual void
  decode(std::ostream& os)
  { return LM::isa->decode(os, m_minsn, vma(), opIndex()); }
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  // Current meaning of 'flags'
  //   0 : short dump (without instructions)
  //   1 : full dump

  std::string
  toString(int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;
  
  void
  ddump() const;
  
  virtual void
  dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  Insn()
  { }

  Insn(const Insn& GCC_ATTR_UNUSED i)
  { }

  Insn&
  operator=(const Insn& GCC_ATTR_UNUSED i)
  { return *this; }
  
protected:
  MachInsn* m_minsn; // pointer to machine instruction [lives in Section]

private:
  VMA m_vma;         // vma of the beginning of this instruction packet
};

} // namespace BinUtil


//***************************************************************************
// CISCInsn
//***************************************************************************

namespace BinUtil {

class CISCInsn : public Insn {
public:
  CISCInsn(MachInsn* minsn, VMA vma, ushort sz)
    : Insn(minsn, vma), m_size(sz)
  { }

  virtual ~CISCInsn() { }
  
  virtual ushort
  size() const
  { return m_size; }

  virtual ushort
  opIndex() const
  { return 0; }
  
  virtual ushort
  numOps() const
  { return 1; }

  // Given a target or branch instruction, returns the target address.
  virtual VMA
  getTargetVMA(VMA vma) const
  { return LM::isa->getInsnTargetVMA(m_minsn, vma, m_size); }

  virtual ushort
  getNumDelaySlots() const
  { return LM::isa->getInsnNumDelaySlots(m_minsn, m_size); }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void
  dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  CISCInsn();

  CISCInsn(const CISCInsn& GCC_ATTR_UNUSED i);

  CISCInsn&
  operator=(const CISCInsn& GCC_ATTR_UNUSED i)
  { return *this; }
  
protected:
private:
  ushort m_size;
};

} // namespace BinUtil


//***************************************************************************
// RISCInsn
//***************************************************************************

namespace BinUtil {

class RISCInsn : public Insn {
public:
  RISCInsn(MachInsn* minsn, VMA vma)
    : Insn(minsn, vma)
  { }

  virtual
  ~RISCInsn()
  { }
  
  virtual ushort
  size() const
  { return LM::isa->getInsnSize(m_minsn); }

  virtual ushort
  opIndex() const
  { return 0; }

  virtual ushort
  numOps() const
  { return 1; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void
  dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  RISCInsn();

  RISCInsn(const RISCInsn& GCC_ATTR_UNUSED i);

  RISCInsn&
  operator=(const RISCInsn& GCC_ATTR_UNUSED i)
  { return *this; }
  
protected:
private:
};

} // namespace BinUtil


//***************************************************************************
// VLIWInsn
//***************************************************************************

namespace BinUtil {

class VLIWInsn : public Insn {
public:
  VLIWInsn(MachInsn* minsn, VMA vma, ushort opIndex)
    : Insn(minsn, vma), m_opIndex(opIndex)
  { }

  virtual ~VLIWInsn()
  { }
  
  virtual ushort
  size() const
  { return LM::isa->getInsnSize(m_minsn); }

  virtual ushort
  opIndex() const
  { return m_opIndex; }

  virtual ushort
  numOps() const
  { return LM::isa->getInsnNumOps(m_minsn); }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void
  dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  VLIWInsn();

  VLIWInsn(const VLIWInsn& GCC_ATTR_UNUSED i);

  VLIWInsn&
  operator=(const VLIWInsn& GCC_ATTR_UNUSED i)
  { return *this; }
  
protected:
private:
  ushort m_opIndex;
};

} // namespace BinUtil

//***************************************************************************

#endif
