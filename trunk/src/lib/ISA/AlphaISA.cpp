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
//    AlphaISA.C
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

#include "AlphaISA.h"

#include <include/gnu_bfd.h>  // for bfd_getl32

#include "instructionSets/alpha.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// AlphaISA
//****************************************************************************

// See 'ISA.h' for comments on the interface

ISA::InstType
AlphaISA::GetInstType(MachInst* mi, ushort opIndex, ushort sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32 inst = (uint32)bfd_getl32((const unsigned char*)mi);
  // FIXME: should we test for little vs. big (same with MIPS)
  
  switch (inst & OP_MASK)
    {
      // Memory Integer/FP Load/Store Instructions (SS 4.2 and 4.8; Tables
      // 4-2 and 4-14)
    case LDA:  // Integer loads
    case LDAH:
    case LDBU:
    case LDL:
    case LDL_L:
    case LDWU:
    case LDF:  // FP loads
    case LDS:
      return (ISA::MEM_LOAD);
    case LDQ:
    case LDQ_L:
    case LDQ_U:
    case LDG:  // FP loads
    case LDT:
    case HW_LD: // PALcode
      return (ISA::MEM_LOAD); // Doubleword
    case STB:  // Integer stores
    case STL:
    case STL_C:
    case STW:
    case STF:  // FP stores
    case STS:
      return (ISA::MEM_STORE);
    case STQ:
    case STQ_C:
    case STQ_U:
    case STG:  // FP stores
    case STT:
    case HW_ST: // PALcode
      return (ISA::MEM_STORE); // Doubleword

      // Integer/FP control Instructions (SS 4.3 and 4.9; Tables 4-3
      // and 4-15)
    case BEQ:  // Integer branch
    case BGE:
    case BGT:
    case BLBC:
    case BLBS:
    case BLE:
    case BLT:
    case BNE:
    case FBEQ: // FP branch
    case FBGE:
    case FBGT:
    case FBLE:
    case FBLT:
    case FBNE:
      return (ISA::BR_COND_REL);
    case BR:
      return (ISA::BR_UN_COND_REL);
    case BSR:  // branch and call
      return (ISA::SUBR_REL);

      // Technically all these instructions are identical except for
      // hints.  We have to assume the compiler's hint is actually
      // what generally happens...      
    case OpJump:   
      switch (inst & MBR_MASK)
	{
	case JMP:
	  return (ISA::BR_UN_COND_IND);
	case JSR:
	case JSR_COROUTINE:  // FIXME: return and call
	  return (ISA::SUBR_IND);
	case RET:
	  return (ISA::SUBR_RET);
	}
      break;
      
      // PALcode format instructions
    case CALLSYS:
      return (ISA::SYS_CALL); 
      
    default:
      break;
    }
  
  return (ISA::OTHER);
}

Addr
AlphaISA::GetInstTargetAddr(MachInst* mi, Addr pc, ushort opIndex, ushort sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32 inst = (uint32)bfd_getl32((const unsigned char*)mi);
  
  psint offset;
  switch (inst & OP_MASK)
    {
      // Integer/FP control Instructions (SS 4.3 and 4.9; Tables 4-3
      // and 4-15)
    case BEQ:  // Integer branch
    case BGE:
    case BGT:
    case BLBC:
    case BLBS:
    case BLE:
    case BLT:
    case BNE:
    case BR:
    case BSR:  
    case FBEQ: // FP branch
    case FBGE:
    case FBGT:
    case FBLE:
    case FBLT:
    case FBNE:
      // Added to the address of the *updated* pc
      offset = BR_DISP(inst);
      if (offset & BR_DISP_SIGN) { offset |= ~BR_DISP_MASK; } // sign extend
      return ((pc + MINST_SIZE) + (offset << 2));  

    case OpJump: // JMP, JSR, JSR_COROUTINE, RET
      break;  // branch content in register
      
    default:
      break;
    }
  
  return (0);
}
