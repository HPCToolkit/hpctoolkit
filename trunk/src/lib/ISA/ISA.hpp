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
//    ISA.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ISA_H 
#define ISA_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>

#include "ISATypes.h"

//*************************** Forward Declarations ***************************

// A one-element decoding cache.  Used to reduce the slowness
// of multiple calls to the disassemblers (especially the GNU ones).
class DecodingCache {
public:
  DecodingCache() { tag = NULL; instSize = 0; }

  // Cache `tag' (Currently, this is just the MachInst*.  Later,
  // we may need secondary tags, such as opIndex).
  MachInst *tag;

  // Cached data.
  // Note, Non-GNU decoders may need more data members (the previous
  // `disassemble_info' struct is used as an implicit cache for the GNU
  // decoders). 
  ushort instSize; 
};

//***************************************************************************
// ISA
//***************************************************************************

// 'ISA': An abstract base class for an Instruction Set Architecture.  

class ISA {
public:
  enum InstType {
    // Types are something like the following
    BR_COND_REL,		// PC-relative conditional branch
    BR_COND_IND,                // Register/Indirect conditional branch
    BR_UN_COND_REL,		// PC-relative unconditional branch
    BR_UN_COND_IND,		// Register/Indirect unconditional branch
    
    SUBR_REL,			// Relative subroutine call
    SUBR_IND,			// Register/Indirect subroutine call
    SUBR_RET,			// Subroutine return
    SYS_CALL,			// System call

    MEM,		        // Memory ref not categorized as load or store
    MEM_LOAD,			// Memory load
    MEM_STORE,			// Memory store

    OTHER,      	       	// Any other valid instruction
    INVALID                     // An invalid instruction
  };

  static const char* InstType2Str(InstType t);

  // --------------------------------------------------------
  
  ISA();
  virtual ~ISA();
  
  // --------------------------------------------------------
  // Instructions:
  // --------------------------------------------------------  

  // Notes:
  //   - Functions with an 'opIndex' parameter: Since instructions are
  //     viewed as a potential VLIW packet, 'opIndex' identifies which
  //     operation we are currently interested in (0-based).  For
  //     non-VLIW instructions, this argument should typically be 0.
  //   - Functions with an optional 'sz' parameter: If already available,
  //     'sz' should be supplied to save an extra call to 'GetInstSize'.
  
  
  // Returns the size (in bytes) of the instruction.  In the case of
  // VLIW instructions, returns the size not of the individual
  // operation but the whole "packet".  For CISC ISAs, if the return
  // value is 0, 'mi' is not a valid instruction.
  virtual ushort GetInstSize(MachInst* mi) = 0;

  // Viewing this instruction as a VLIW packet, return the maximum
  // number of operations within the packet.  For non-VLIW
  // instructions, the return value will typically be 1.  A return
  // value of 0 indicates the 'packet' contains data.
  virtual ushort GetInstNumOps(MachInst* mi) = 0;
  
  // Returns the instruction type, a general classification of the
  // operation performed
  virtual InstType GetInstType(MachInst* mi, ushort opIndex, ushort sz = 0)
    = 0;
  
  // Given a jump or branch instruction 'mi', return the target address.
  // If a target cannot be computed, return 0.  Note that a target is
  // not computed when it depends on values in registers
  // (e.g. indirect jumps).  'pc' is used only to calculate
  // PC-relative targets.
  virtual Addr GetInstTargetAddr(MachInst* mi, Addr pc, ushort opIndex,
				 ushort sz = 0) = 0;

  // Returns the number of delay slots that must be observed by
  // schedulers before the effect of instruction 'mi' can be
  // assumed to be fully obtained (e.g., RISC braches).
  virtual ushort GetInstNumDelaySlots(MachInst* mi, ushort opIndex,
				      ushort sz = 0) = 0;
  
  // Returns whether or not the instruction 'mi1' "explicitly"
  // executes in parallel with its successor 'mi2' (successor in the
  // sequential sense).  IOW, this has special reference to
  // "explicitly parallel" architecture, not superscalar design.
  virtual bool IsParallelWithSuccessor(MachInst* mi1, ushort opIndex1,
				       ushort sz1,
				       MachInst* mi2, ushort opIndex2,
				       ushort sz2) const = 0;

  // ConvertPCToOpPC: Given a pc at the beginning of an instruction
  // and an opIndex, returns one value -- an 'operation pc' --
  // representing both. 
  //
  // ConvertOpPCToPC: Given an 'operation pc', returns the individual
  // pc and opIndex components. (The latter is returned as a
  // pass-by-reference parameter.)  N.B.: The 'operation pc' must
  // follow the convetions of 'ConvertPCToOpPC'.
  //
  // Sometimes users need to pretend that the individual operations in
  // VLIW instructions are addressable.  This is, of course not true,
  // but 'operation pcs' are useful for [pc->xxx] maps (e.g. debugging
  // information is stored in this manner).
  //
  // The default function assumes non-VLIW architecture
  virtual Addr ConvertPCToOpPC(Addr pc, ushort opIndex) const
  { return pc; }

  virtual Addr ConvertOpPCToPC(Addr oppc, ushort& opIndex) const
  { opIndex = 0; return oppc; }
  
private:
  // Should not be used
  ISA(const ISA& i) { }
  ISA& operator=(const ISA& i) { return *this; }

protected:
  DecodingCache *CacheLookup(MachInst* cmi) {
    if (cmi == _cache->tag) {
      return _cache;
    } else {
      _cache->tag = NULL;
      return NULL;
    }
  }
  void CacheSet(MachInst* cmi, ushort size) {
    _cache->tag = cmi; _cache->instSize = size;
  }

private:
  DecodingCache *_cache;
};

//****************************************************************************

#endif 
