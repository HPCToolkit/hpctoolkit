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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef binutils_Insn_hpp 
#define binutils_Insn_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "LM.hpp"

#include <lib/isa/ISA.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// Insn
//***************************************************************************

namespace binutils {

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
  Insn(MachInsn* _minsn) 
    : minsn(_minsn), vma(0) { } 

  Insn(MachInsn* _minsn, VMA _vma) 
    : minsn(_minsn), vma(_vma) { } 

  virtual ~Insn() { }
  
  // Returns a classification of the instruction
  ISA::InsnDesc 
  GetDesc() const {
    return LM::isa->GetInsnDesc(minsn, GetOpIndex(), GetSize());
  }
  
  // 'GetBits' returns a pointer to the bits of the instruction;
  // 'GetSize' returns the size of the machine instruction.
  // In the case of VLIW instructions, returns the {bits, size} not of
  // the individual operation but the whole "packet".
  virtual MachInsn* 
  GetBits() const { return minsn; }

  virtual ushort    
  GetSize() const = 0;

  // Returns the VMA for the beginning of this instruction.
  // WARNING: this is unrelocated
  VMA 
  GetVMA() const { return vma; }
  
  void 
  SetVMA(VMA _vma) { vma = _vma; }

  // Returns the end vma, i.e. the vma immediately following this instruction
  VMA 
  GetEndVMA() const 
    { return GetVMA() + GetSize(); }

  // Returns the operation VMA for the beginning of this instruction
  VMA GetOpVMA() const 
    { return LM::isa->ConvertVMAToOpVMA(GetVMA(), GetOpIndex()); }
  
  // Viewing each object code instruction as an instruction packet,
  // and recalling that all packets are "unpacked" in a 'LM',
  // 'GetOpIndex' retuns the (0 based) index of this instruction in
  // the original packet.  'GetNumOps' returns the number of
  // operations in this packet.  For RISC and CISC machines these
  // values will be 0 and 1, respectively.
  virtual ushort 
  GetOpIndex() const = 0;

  virtual ushort 
  GetNumOps() const = 0; 

  // Returns the target address of a jump or branch instruction. If a
  // target cannot be computed, return 0.  Note that a target is not
  // computed when it depends on values in registers (e.g. indirect
  // jumps).  'vma' is used only to calculate PC-relative targets.
  virtual VMA 
  GetTargetVMA(VMA _vma) const {
    return LM::isa->GetInsnTargetVMA(minsn, _vma, GetOpIndex(), GetSize());
  }
  
  // Returns the number of delay slots that must be observed by
  // schedulers before the effect of instruction 'mi' can be
  // assumed to be fully obtained (e.g., RISC braches).
  virtual ushort 
  GetNumDelaySlots() const {
    return LM::isa->GetInsnNumDelaySlots(minsn, GetOpIndex(), GetSize());
  }

  // Returns whether or not the instruction "explicitly" executes in
  // parallel with its successor 'mi2' (successor in the sequential
  // sense).  IOW, this has special reference to "explicitly parallel"
  // architecture, not superscalar design.
  virtual bool 
  IsParallelWithSuccessor(Insn* mi2) const {
    return LM::isa->IsParallelWithSuccessor(minsn, GetOpIndex(), GetSize(),
					    mi2->GetBits(), mi2->GetOpIndex(),
					    mi2->GetSize());
  }

  // decode: 
  virtual void 
  decode(std::ostream& os) {
    return LM::isa->decode(os, minsn, GetVMA(), GetOpIndex());
  }
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  // Current meaning of 'flags'
  //   0 : short dump (without instructions)
  //   1 : full dump

  std::string toString(int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = LM::DUMP_Short, const char* pre = "") const;
  
  void ddump() const;
  
  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  Insn() { }
  Insn(const Insn& i) { }
  Insn& operator=(const Insn& i) { return *this; }
  
protected:
  MachInsn* minsn; // pointer to machine instruction [lives in Section]

private:
  VMA vma;         // vma of the beginning of this instruction packet
};

} // namespace binutils


//***************************************************************************
// CISCInsn
//***************************************************************************

namespace binutils {

class CISCInsn : public Insn {
public:
  CISCInsn(MachInsn* _minsn, VMA _vma, ushort sz)
    : Insn(_minsn, _vma), size(sz) { }    

  virtual ~CISCInsn() { }
  
  virtual ushort GetSize() const { return size; }
  virtual ushort GetOpIndex() const { return 0; }
  virtual ushort GetNumOps() const  { return 1; }

  // Given a target or branch instruction, returns the target address.
  virtual VMA GetTargetVMA(VMA _vma) const {
    return LM::isa->GetInsnTargetVMA(minsn, _vma, size);
  }

  virtual ushort GetNumDelaySlots() const {
    return LM::isa->GetInsnNumDelaySlots(minsn, size);
  }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = LM::DUMP_Short, const char* pre = "") const;
  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  CISCInsn();
  CISCInsn(const CISCInsn& i);
  CISCInsn& operator=(const CISCInsn& i) { return *this; }
  
protected:
private:
  ushort size;
};

} // namespace binutils


//***************************************************************************
// RISCInsn
//***************************************************************************

namespace binutils {

class RISCInsn : public Insn {
public:
  RISCInsn(MachInsn* _minsn, VMA _vma)
    : Insn(_minsn, _vma) { } 

  virtual ~RISCInsn() { }
  
  virtual ushort GetSize() const { return LM::isa->GetInsnSize(minsn); }
  virtual ushort GetOpIndex() const { return 0; }

  virtual ushort GetNumOps() const  { return 1; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = LM::DUMP_Short, const char* pre = "") const;
  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  RISCInsn();
  RISCInsn(const RISCInsn& i);
  RISCInsn& operator=(const RISCInsn& i) { return *this; }
  
protected:
private:
};

} // namespace binutils


//***************************************************************************
// VLIWInsn
//***************************************************************************

namespace binutils {

class VLIWInsn : public Insn {
public:
  VLIWInsn(MachInsn* _minsn, VMA _vma, ushort _opIndex)
    : Insn(_minsn, _vma), opIndex(_opIndex) { } 

  virtual ~VLIWInsn() { }
  
  virtual ushort GetSize() const 
    { return LM::isa->GetInsnSize(minsn); }

  virtual ushort GetOpIndex() const 
    { return opIndex; }

  virtual ushort GetNumOps() const  
    { return LM::isa->GetInsnNumOps(minsn); }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = LM::DUMP_Short, const char* pre = "") const;
  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  VLIWInsn();
  VLIWInsn(const VLIWInsn& i);
  VLIWInsn& operator=(const VLIWInsn& i) { return *this; }
  
protected:
private:
  ushort opIndex;
};

} // namespace binutils

//***************************************************************************

#endif 
