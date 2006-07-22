// -*-Mode: C++;-*-
// $Id$
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
//    Instruction.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Instruction_H 
#define Instruction_H

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "LoadModule.hpp"
#include <lib/ISA/ISA.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// Instruction
//***************************************************************************

// 'Instruction' is an abstract class that represents one single
// processor instruction instruction in the 'TextSection' of a
// 'LoadModule'.  For the sake of generality, all instructions are
// viewed as (potentially) variable sized instruction packets.
//
// VLIW instructions (or 'packets') that consist of multiple
// operations, are "unpacked" or "flattened" into their component
// operations; thus each operation is an 'Instruction'.

class Instruction {
public:
  Instruction(MachInst* _minst) : minst(_minst), vma(0) { } 
  Instruction(MachInst* _minst, Addr _vma) : minst(_minst), vma(_vma) { } 
  virtual ~Instruction() { }
  
  // Returns a classification of the instruction
  ISA::InstDesc GetDesc() const {
    return isa->GetInstDesc(minst, GetOpIndex(), GetSize());
  }
  
  // 'GetBits' returns a pointer to the bits of the instruction;
  // 'GetSize' returns the size of the machine instruction.
  // In the case of VLIW instructions, returns the {bits, size} not of
  // the individual operation but the whole "packet".
  virtual MachInst* GetBits() const { return minst; }
  virtual ushort    GetSize() const = 0;

  // Returns the VMA for the beginning of this instruction.
  // WARNING: this is unrelocated
  Addr GetVMA() const { return vma; }
  void SetVMA(Addr _vma) { vma = _vma; }
  
  // Viewing each object code instruction as an instruction packet,
  // and recalling that all packets are "unpacked" in a 'LoadModule',
  // 'GetOpIndex' retuns the (0 based) index of this instruction in
  // the original packet.  'GetNumOps' returns the number of
  // operations in this packet.  For RISC and CISC machines these
  // values will be 0 and 1, respectively.
  virtual ushort GetOpIndex() const = 0;
  virtual ushort GetNumOps() const = 0; 

  // Returns the target address of a jump or branch instruction. If a
  // target cannot be computed, return 0.  Note that a target is not
  // computed when it depends on values in registers (e.g. indirect
  // jumps).  'vma' is used only to calculate PC-relative targets.
  virtual Addr GetTargetAddr(Addr _vma) const {
    return isa->GetInstTargetAddr(minst, _vma, GetOpIndex(), GetSize());
  }
  
  // Returns the number of delay slots that must be observed by
  // schedulers before the effect of instruction 'mi' can be
  // assumed to be fully obtained (e.g., RISC braches).
  virtual ushort GetNumDelaySlots() const {
    return isa->GetInstNumDelaySlots(minst, GetOpIndex(), GetSize());
  }

  // Returns whether or not the instruction "explicitly" executes in
  // parallel with its successor 'mi2' (successor in the sequential
  // sense).  IOW, this has special reference to "explicitly parallel"
  // architecture, not superscalar design.
  virtual bool IsParallelWithSuccessor(Instruction* mi2) const {
    return isa->IsParallelWithSuccessor(minst, GetOpIndex(), GetSize(),
					mi2->GetBits(), mi2->GetOpIndex(),
					mi2->GetSize());
  }
  
  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  Instruction() { }
  Instruction(const Instruction& i) { }
  Instruction& operator=(const Instruction& i) { return *this; }
  
protected:
  MachInst*  minst; // pointer to machine instruction [lives in Section]
private:
  Addr vma;         // vma of the beginning of this instruction packet
};

//***************************************************************************
// CISCInstruction
//***************************************************************************

class CISCInstruction : public Instruction {
public:
  CISCInstruction(MachInst* _minst, Addr _vma, ushort sz)
    : Instruction(_minst, _vma), size(sz) { }    

  virtual ~CISCInstruction() { }
  
  virtual ushort GetSize() const { return size; }
  virtual ushort GetOpIndex() const { return 0; }
  virtual ushort GetNumOps() const  { return 1; }

  // Given a target or branch instruction, returns the target address.
  virtual Addr GetTargetAddr(Addr _vma) const {
    return isa->GetInstTargetAddr(minst, _vma, size);
  }

  virtual ushort GetNumDelaySlots() const {
    return isa->GetInstNumDelaySlots(minst, size);
  }

  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  CISCInstruction();
  CISCInstruction(const CISCInstruction& i);
  CISCInstruction& operator=(const CISCInstruction& i) { return *this; }
  
protected:
private:
  ushort size;
};

//***************************************************************************
// RISCInstruction
//***************************************************************************

class RISCInstruction : public Instruction {
public:
  RISCInstruction(MachInst* _minst, Addr _vma)
    : Instruction(_minst, _vma) { } 

  virtual ~RISCInstruction() { }
  
  virtual ushort GetSize() const { return isa->GetInstSize(minst); }
  virtual ushort GetOpIndex() const { return 0; }
  virtual ushort GetNumOps() const  { return 1; }

  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  RISCInstruction();
  RISCInstruction(const RISCInstruction& i);
  RISCInstruction& operator=(const RISCInstruction& i) { return *this; }
  
protected:
private:
};

//***************************************************************************
// VLIWInstruction
//***************************************************************************

class VLIWInstruction : public Instruction {
public:
  VLIWInstruction(MachInst* _minst, Addr _vma, ushort _opIndex)
    : Instruction(_minst, _vma), opIndex(_opIndex) { } 

  virtual ~VLIWInstruction() { }
  
  virtual ushort GetSize() const { return isa->GetInstSize(minst); }
  virtual ushort GetOpIndex() const { return opIndex; }
  virtual ushort GetNumOps() const  { return isa->GetInstNumOps(minst); }

  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  // Should not be used
  VLIWInstruction();
  VLIWInstruction(const VLIWInstruction& i);
  VLIWInstruction& operator=(const VLIWInstruction& i) { return *this; }
  
protected:
private:
  ushort opIndex;
};

//***************************************************************************

#endif 
