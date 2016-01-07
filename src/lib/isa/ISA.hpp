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
//    ISA.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_ISA_hpp
#define isa_ISA_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************


#include <include/gcc-attr.h>
#include <include/uint.h>
#include <include/gnu_dis-asm.h>

#include "ISATypes.hpp"

//*************************** Forward Declarations ***************************

// A one-element decoding cache.  Used to reduce the slowness
// of multiple calls to the disassemblers (especially the GNU ones).
class DecodingCache {
public:
  DecodingCache()
  { tag = NULL; insnSize = 0; }

  // Cache `tag' (Currently, this is just the MachInsn*.  Later,
  // we may need secondary tags, such as opIndex).
  MachInsn *tag;

  // Cached data.
  // Note, Non-GNU decoders may need more data members (the previous
  // `disassemble_info' struct is used as an implicit cache for the GNU
  // decoders).
  ushort insnSize;
};

//*************************** Forward Declarations ***************************


//***************************************************************************
// ISA
//***************************************************************************

// 'ISA': An abstract base class for an Instruction Set Architecture.
// It can classify and return certain information about the actual
// bits of a machine instruction.
class ISA {

  // ------------------------------------------------------------------------


public:

  // ------------------------------------------------------------------------

  // InsnDesc: Describes an instruction's class in various levels.
  // Note: This should not have virtual functions so that objects can be
  // passed around by value.
  class InsnDesc {
  public:

    // Used to implement InsnDesc.  These are mutually exclusive
    // instruction classes.  They are not intended for general use!
    enum IType {
      // Note: only the Alpha ISA can reliably uses all categories.
      // The other ISAs -- with the exception of MIPS -- only classify
      // branches and do not distinguish between FP/int branches.

      MEM_LOAD,                 // Memory load (integer and FP)
      MEM_STORE,                // Memory store (integer and FP)
      MEM_OTHER,                // Other memory insn

      INT_BR_COND_REL,          // PC-relative conditional branch (int compare)
      INT_BR_COND_IND,          // Register/Indirect conditional branch
      FP_BR_COND_REL,           // PC-relative conditional branch (FP compare)
      FP_BR_COND_IND,           // Register/Indirect conditional branch
      
      BR_UN_COND_REL,           // PC-relative unconditional branch
      BR_UN_COND_IND,           // Register/Indirect unconditional branch
      
      SUBR_REL,	                // Relative subroutine call
      SUBR_IND,	                // Register/Indirect subroutine call
      SUBR_RET,	                // Subroutine return

      // ----------------
      // The Alpha ISA classifies OTHER instructions into the
      // following subdivisions.
      INT_ADD,      // Integer operations
      INT_SUB,
      INT_MUL,
      INT_CMP,
      INT_LOGIC,
      INT_SHIFT,
      INT_MOV,
      INT_OTHER,
      
      FP_ADD,       // FP operation
      FP_SUB,
      FP_MUL,
      FP_DIV,
      FP_CMP,
      FP_CVT,
      FP_SQRT,
      FP_MOV,
      FP_OTHER,

      // OTHER: a non-integer, non-fp instruction
      // ----------------

      SYS_CALL,	                // System call
      OTHER,                    // Any other valid instruction
      INVALID                   // An invalid instruction
    };

  public:
    // A 'InsnDesc' can be created using the bit definitions above.
    InsnDesc(IType t = INVALID)
      : ty(t)
    { }
    
    ~InsnDesc()
    { }

    InsnDesc(const InsnDesc& x)
    { *this = x; }
    
    InsnDesc&
    operator=(const InsnDesc& x)
    {
      ty = x.ty;
      return *this;
    }

    // -----------------------------------------------------

    // IsValid: A valid instruction
    bool
    isValid() const
    { return ty != INVALID; }

    // An invalid instruction
    bool
    isInvalid() const
    { return ty == INVALID; }

    // -----------------------------------------------------

    // Memory load
    bool
    isMemLoad() const
    { return ty == MEM_LOAD; }

    // Memory store
    bool
    isMemStore() const
    { return ty == MEM_STORE; }

    // Memory ref not categorized as load or store
    bool
    isMemOther() const
    { return ty == MEM_OTHER; }

    // Any memory operation
    bool
    isMemOp()
    { return isMemLoad() || isMemStore() || isMemOther(); }

    // -----------------------------------------------------

    // Some sort of branch
    bool
    isBr() const
    { return isBrRel() || isBrInd(); }

    // Some sort of PC-relative branch
    bool
    isBrRel() const
    { return isBrCondRel() || isBrUnCondRel(); }

    // Some sort of Register/Indirect branch
    bool
    isBrInd() const
    { return isBrCondInd() || isBrUnCondInd(); }

    // PC-relative conditional branch
    bool
    isBrCondRel() const
    { return ty == INT_BR_COND_REL || ty == FP_BR_COND_REL; }

    // Register/Indirect conditional branch
    bool
    isBrCondInd() const
    { return ty == INT_BR_COND_IND || ty == FP_BR_COND_IND; }

    // PC-relative unconditional branch
    bool
    isBrUnCondRel() const
    { return ty == BR_UN_COND_REL; }

    // Register/Indirect unconditional branch
    bool
    isBrUnCondInd() const
    { return ty == BR_UN_COND_IND; }

    // A branch with floating point comparison
    bool
    isFPBr() const
    { return ty == FP_BR_COND_REL || ty == FP_BR_COND_IND; }

    // -----------------------------------------------------

    // Some subroutine call (but not a system call)
    bool
    isSubr() const
    { return isSubrRel() || isSubrInd(); }

    // Relative subroutine call
    bool
    isSubrRel() const
    { return ty == SUBR_REL; }

    // Register/Indirect subroutine call
    bool
    isSubrInd() const
    { return ty == SUBR_IND; }

    // Subroutine return
    bool
    isSubrRet() const
    { return ty == SUBR_RET; }

    // -----------------------------------------------------

    // A floating point instruction
    bool
    isFP() const
    { return isFPBr() || isFPArith() || isFPOther(); }

    // A floating point arithmetic instruction (excludes fp branches,
    // moves, other)
    bool
    isFPArith() const
    {
      return (ty == FP_ADD || ty == FP_ADD || ty == FP_SUB || ty == FP_MUL
	      || ty == FP_DIV || ty == FP_CMP || ty == FP_CVT
	      || ty == FP_SQRT);
    }

    // Any other floating point instruction
    bool
    isFPOther() const
    { return ty == FP_MOV || ty == FP_OTHER; }

    // -----------------------------------------------------

    // An integer arithmetic instruction (excludes branches, moves, other)
    bool
    isIntArith() const
    {
      return (ty == INT_ADD || ty == INT_SUB || ty == INT_MUL
	      || ty == INT_CMP || ty == INT_LOGIC || ty == INT_SHIFT);
    }

    // Any other integer instruction
    bool
    isIntOther() const
    { return ty == INT_MOV || ty == INT_OTHER; }

    // Any integer instruction
    bool
    isIntOp() const
    { return isIntArith() || isIntOther(); }

    // -----------------------------------------------------

    // System call
    bool
    isSysCall() const
    { return ty == SYS_CALL; }

    // Any other valid instruction
    bool
    isOther() const
    { return ty == OTHER; }

    // -----------------------------------------------------

    void
    set(IType t)
    { ty = t; }

    const char*
    toString() const;

    void
    dump(std::ostream& o = std::cerr);
    
    void
    ddump();

  private:
    IType ty;
  };

  // ------------------------------------------------------------------------

public:

  ISA();
  virtual ~ISA();

  // --------------------------------------------------------
  // Reference Counting:
  // --------------------------------------------------------

  // Because the only real member data an ISA has is a decoding cache,
  // one typically does not really want to make copies of an ISA
  // object.  However, there are times when it may be convenient to
  // hand out several pointers to an ISA, creating a memory management
  // nightmare in which ownership is not clearly demarcated.  We
  // therefore provide an optional basic reference counting mechanism,
  // a slightly less ghoulish solution.

  void
  attach()
  { ++refcount; }
  
  void
  detach()
  {
    if (--refcount == 0) {
      delete this;
    }
  }

  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------

  // Notes:
  //   - Functions with an 'opIndex' parameter: Since instructions are
  //     viewed as a potential VLIW packet, 'opIndex' identifies which
  //     operation we are currently interested in (0-based).  For
  //     non-VLIW instructions, this argument should typically be 0.
  //   - Functions with an optional 'sz' parameter: If already available,
  //     'sz' should be supplied to save an extra call to 'getInsnSize'.


  // Returns the size (in bytes) of the instruction.  In the case of
  // VLIW instructions, returns the size not of the individual
  // operation but the whole "packet".  For CISC ISAs, if the return
  // value is 0, 'mi' is not a valid instruction.
  virtual ushort
  getInsnSize(MachInsn* mi) = 0;

  // Viewing this instruction as a VLIW packet, return the maximum
  // number of operations within the packet.  For non-VLIW
  // instructions, the return value will typically be 1.  A return
  // value of 0 indicates the 'packet' contains data.
  virtual ushort
  getInsnNumOps(MachInsn* mi) = 0;

  // Returns an instruction descriptor which provides a high level
  // classifications of the operation performed
  virtual InsnDesc
  getInsnDesc(MachInsn* mi, ushort opIndex, ushort sz = 0) = 0;

  // Given a jump or branch instruction 'mi', return the target address.
  // If a target cannot be computed, return 0.  Note that a target is
  // not computed when it depends on values in registers
  // (e.g. indirect jumps).  'vma' is used only to calculate
  // PC-relative targets.
  virtual VMA
  getInsnTargetVMA(MachInsn* mi, VMA vma, ushort opIndex, ushort sz = 0) = 0;

  // Returns the number of delay slots that must be observed by
  // schedulers before the effect of instruction 'mi' can be
  // assumed to be fully obtained (e.g., RISC braches).
  virtual ushort
  getInsnNumDelaySlots(MachInsn* mi, ushort opIndex, ushort sz = 0) = 0;

  // Returns whether or not the instruction 'mi1' "explicitly"
  // executes in parallel with its successor 'mi2' (successor in the
  // sequential sense).  IOW, this has special reference to
  // "explicitly parallel" architecture, not superscalar design.
  virtual bool
  isParallelWithSuccessor(MachInsn* mi1, ushort opIndex1,
			  ushort sz1,
			  MachInsn* mi2, ushort opIndex2,
			  ushort sz2) const = 0;

  // decode:
  virtual void
  decode(std::ostream& os, MachInsn* mi, VMA vma, ushort opIndex) = 0;

  // convertVMAToOpVMA: Given a vma at the beginning of an instruction
  // and an opIndex, returns one value -- an 'operation vma' --
  // representing both.
  //
  // convertOpVMAToVMA: Given an 'operation vma', returns the individual
  // vma and opIndex components. (The latter is returned as a
  // pass-by-reference parameter.)  N.B.: The 'operation vma' must
  // follow the convetions of 'convertVMAToOpVMA'.
  //
  // Sometimes users need to pretend that the individual operations in
  // VLIW instructions are addressable.  This is, of course not true,
  // but 'operation vmas' are useful for [vma->xxx] maps (e.g. debugging
  // information is stored in this manner).
  //
  // The default function assumes non-VLIW architecture
  virtual VMA
  convertVMAToOpVMA(VMA vma, ushort GCC_ATTR_UNUSED opIndex) const
  { return vma; }

  virtual VMA
  convertOpVMAToVMA(VMA opvma, ushort& opIndex) const
  { opIndex = 0; return opvma; }

private:
  // Should not be used
  ISA(const ISA& GCC_ATTR_UNUSED i)
  { }

  ISA&
  operator=(const ISA& GCC_ATTR_UNUSED i)
  { return *this; }

protected:
  DecodingCache*
  cacheLookup(MachInsn* cmi)
  {
    if (cmi == _cache->tag) {
      return _cache;
    }
    else {
      _cache->tag = NULL;
      return NULL;
    }
  }

  void
  cacheSet(MachInsn* cmi, ushort size)
  { _cache->tag = cmi; _cache->insnSize = size; }

private:
  DecodingCache *_cache;
  unsigned int refcount;
};


//***************************************************************************
// binutils helpers
//***************************************************************************

class GNUbu_disdata {
public:
  MachInsn* insn_addr; // memory address of insn
  VMA       insn_vma;  // vma of insn
};


extern "C" {

  int
  GNUbu_fprintf(void* stream, const char* format, ...);

  int
  GNUbu_fprintf_stub(void* stream, const char* format, ...);

  void
  GNUbu_print_addr_stub(bfd_vma di_vma, struct disassemble_info* di);

  int
  GNUbu_read_memory(bfd_vma vma, bfd_byte* myaddr, unsigned int len,
		    struct disassemble_info* di);

} // extern "C"


//****************************************************************************



#endif // isa_ISA_hpp
