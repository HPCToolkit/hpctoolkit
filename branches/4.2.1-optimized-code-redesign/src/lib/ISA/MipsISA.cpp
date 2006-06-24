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
//    MipsISA.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "MipsISA.hpp"

#include <include/gnu_bfd.h>  // for bfd_getb32

#include "instructionSets/mips.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// MipsISA
//****************************************************************************

// See 'ISA.h' for comments on the interface

ISA::InstDesc
MipsISA::GetInstDesc(MachInst* mi, ushort opIndex, ushort sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t inst = (uint32_t)bfd_getb32((const unsigned char*)mi);
  
  switch (inst & OP_MASK)
    {
    case OPSpecial:
      switch (inst & OPSpecial_MASK)
	{
	case JR:                        // Instructions from Table A-13
	  // JR $31 returns from a JAL call instruction.
	  if (REG_S(inst) == REG_RA) {
	    return InstDesc(InstDesc::SUBR_RET);
	  } else {
	    return InstDesc(InstDesc::BR_UN_COND_IND);
	  }
	case JALR:
	  return InstDesc(InstDesc::SUBR_IND);
	  
	case SYSCALL:                   // Instructions from Table A-16, 
	case BREAK:                     //   Table A-17
	case TGE:   // Trap-on-Condition...
	case TGEU:
	case TLT:
	case TLTU:
	case TEQ:
	case TNE:
	  return InstDesc(InstDesc::SYS_CALL); 

	case SYNC:
	  return InstDesc(InstDesc::OTHER);
	}
      break;
    case OPRegImm:
      switch (inst & OPRegImm_MASK)
	{
	case BLTZ:                      // Instructions from Table A-15
	case BGEZ:
	case BLTZL:
	case BGEZL:
	  return InstDesc(InstDesc::INT_BR_COND_REL);
	  
	case BLTZAL:  // Link
	case BGEZAL:  // Link
	case BLTZALL: // Link
	case BGEZALL: // Link
	  return InstDesc(InstDesc::SUBR_REL); 

	case TGEI:                      // Instructions from Table A-18 
	case TGEIU: // Trap-on-Condition...
	case TLTI:
	case TLTIU:
	case TEQI:
	case TNEI:
	  return InstDesc(InstDesc::OTHER); 
	}
      break;
    case OPCop1x:
      switch (inst & OPCop1x_MASK)
	{
	case LWXC1: // Word	        // Instructions from Table A-7
	  return InstDesc(InstDesc::MEM_LOAD);
	case LDXC1: // Doubleword
	  return InstDesc(InstDesc::MEM_LOAD);
	case SWXC1: // Word
	  return InstDesc(InstDesc::MEM_STORE);
	case SDXC1: // Doubleword
	  return InstDesc(InstDesc::MEM_STORE);
	}
      break;

    // Normal instruction class
    case LB:   // Byte                  // Instructions from Table A-3
    case LBU:  // Byte Unsigned         //  Table A-4, Table A-5, Table A-6
    case LH:   // Halfword
    case LHU:  // Halfword Unsigned
    case LW:   // Word
    case LWU:  // Word Unsigned
    case LWL:  // Word Left
    case LWR:  // Word Right
    case LL:   // Linked Word
    case LWC1: // Word
    case LWC2: // Word
      return InstDesc(InstDesc::MEM_LOAD);

    case LD:   // Doubleword
    case LDL:  // Doubleword Left
    case LDR:  // Doubleword Right
    case LLD:  // Linked Doubleword
    case LDC1: // Doubleword
    case LDC2: // Doubleword
      return InstDesc(InstDesc::MEM_LOAD); // Doubleword

    case SB:   // Byte
    case SH:   // Halfword
    case SW:   // Word
    case SWL:  // Word Left
    case SWR:  // Word Right
    case SC:   // Conditional Word
    case SWC1: // Word
    case SWC2: // Word
      return InstDesc(InstDesc::MEM_STORE);
      
    case SD:   // Doubleword
    case SDL:  // Doubleword Left
    case SDR:  // Doubleword Right
    case SCD:  // Conditional Doubleword
    case SDC1: // Doubleword
    case SDC2: // Doubleword
      return InstDesc(InstDesc::MEM_STORE); // Doubleword

                                        // Instructions from Table A-12
      // N.B.: These instructions are PC-region but we will treat them
      // as PC-relative
    case J:                             
      return InstDesc(InstDesc::BR_UN_COND_REL);  
    case JAL:                           
      return InstDesc(InstDesc::SUBR_REL);     
      
    case BEQ:                           // Instructions from Table A-14
      // If operands are the same then then there is no fall-thru branch.
      // Test for general case.
      if (REG_S(inst) == REG_T(inst))
	return InstDesc(InstDesc::BR_UN_COND_REL);
      else
	return InstDesc(InstDesc::INT_BR_COND_REL);
    case BNE:
    case BLEZ:
    case BGTZ:
    case BEQL:
    case BNEL:
    case BLEZL:
    case BGTZL:
      return InstDesc(InstDesc::INT_BR_COND_REL);

    default:
      break;
    }
  
  return InstDesc(InstDesc::OTHER);
}


Addr
MipsISA::GetInstTargetAddr(MachInst* mi, Addr pc, ushort opIndex, ushort sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t inst = (uint32_t)bfd_getb32((const unsigned char*)mi);

  psint offset;
  switch (inst & OP_MASK)
    {
    case OPSpecial:
      switch (inst & OPSpecial_MASK)
	{
	case JR:                        // Instructions from Table A-13
	case JALR:
	  break;  // branch content in register
	}
      break;
    case OPRegImm:
      switch (inst & OPRegImm_MASK)
	{
	case BLTZ:                      // Instructions from Table A-15
	case BGEZ:
	case BLTZAL:  
	case BGEZAL:  
	case BLTZL:
	case BGEZL:
	case BLTZALL: 
	case BGEZALL:
	  // Added to the address of the instruction *following* the branch
	  offset = IMM(inst);
	  if (offset & IMM_SIGN) { offset |= ~IMM_MASK; } // sign extend
	  return ((pc + MINST_SIZE) + (offset << 2));
	}
      break;

    // Normal instruction class
    case J:                             // Instructions from Table A-12
    case JAL:
      // These are PC-Region and not PC-Relative instructions.
      // Upper order bits come from the address of the delay-slot instruction
      offset = INST_INDEX(inst) << 2;
      return (((pc + MINST_SIZE) & INST_INDEX_UPPER_MASK) | offset);
      
    case BEQ:                           // Instructions from Table A-14
    case BNE:
    case BLEZ:
    case BGTZ:
    case BEQL:
    case BNEL:
    case BLEZL:
    case BGTZL:
      // Added to the address of the instruction *following* the branch
      offset = IMM(inst);
      if (offset & IMM_SIGN) { offset |= ~IMM_MASK; } // sign extend
      return ((pc + MINST_SIZE) + (offset << 2));
      
    default:
      break;
    }
  
  return (0);
}

ushort
MipsISA::GetInstNumDelaySlots(MachInst* mi, ushort opIndex, ushort sz)
{ 
  // All branches have an architectural delay of one
  // instruction. Treat branch-likely instructions as regular
  // branches.

  // (We don't care about delays on instructions such as loads/stores)
  InstDesc d = GetInstDesc(mi, opIndex, sz);
  if (d.IsBr() || d.IsSubr() || d.IsSubrRet()) {
    return 1;
  } else {
    return 0;
  }
}

//****************************************************************************

#if 0

#if (__sgi)
# include <disassembler.h>
#endif

// dis(1)

void VerifyInstructionDecoding(/**/)
{

}

#endif
