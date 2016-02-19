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

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

//*************************** User Include Files ****************************

#include <include/gnu_bfd.h>  // for bfd_getl32

#include "AlphaISA.hpp"

#include <lib/isa-lean/alpha/instruction-set.h>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// AlphaISA
//****************************************************************************

// See 'ISA.h' for comments on the interface

ISA::InsnDesc
AlphaISA::getInsnDesc(MachInsn* mi, ushort GCC_ATTR_UNUSED opIndex,
		      ushort GCC_ATTR_UNUSED sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t insn = (uint32_t)bfd_getl32((const unsigned char*)mi);
  // FIXME: should we test for little vs. big (same with MIPS)
  
  switch (insn & OP_MASK)
    {
      // ---------------------------------------------------
      // Memory Load/Store Instructions (SS 4.2 & 4.8; Tables 4-2 & 4-14)
      // ---------------------------------------------------
    case LDA:  // Integer loads (Table 4-2)
    case LDAH:
    case LDBU:
    case LDL:
    case LDL_L:
    case LDQ:
    case LDQ_L:
    case LDQ_U:
    case LDWU:
      return InsnDesc(InsnDesc::MEM_LOAD);
      
    case STB:  // Integer stores (Table 4-2)
    case STL:
    case STL_C:
    case STQ:
    case STQ_C:
    case STQ_U:
    case STW:
      return InsnDesc(InsnDesc::MEM_STORE);

    case LDF:  // FP loads (Table 4-14)
    case LDG:
    case LDS:
    case LDT:
      return InsnDesc(InsnDesc::MEM_LOAD);

    case STF:  // FP stores (Table 4-14)
    case STG:
    case STS:
    case STT:
      return InsnDesc(InsnDesc::MEM_STORE);

    case HW_LD: // PALcode
      return InsnDesc(InsnDesc::MEM_LOAD); // Doubleword

    case HW_ST: // PALcode
      return InsnDesc(InsnDesc::MEM_STORE); // Doubleword

      // ---------------------------------------------------
      // Control Instructions (SS 4.3 & 4.9; Tables 4-3 & 4-15)
      // ---------------------------------------------------
    case BEQ:  // Integer branch (Table 4-3)
    case BGE:
    case BGT:
    case BLBC:
    case BLBS:
    case BLE:
    case BLT:
    case BNE:
      return InsnDesc(InsnDesc::INT_BR_COND_REL);

    case FBEQ: // FP branch (Table 4-15)
    case FBGE:
    case FBGT:
    case FBLE:
    case FBLT:
    case FBNE:
      return InsnDesc(InsnDesc::FP_BR_COND_REL);
      
    case BR:   // (Table 4-3)
      return InsnDesc(InsnDesc::BR_UN_COND_REL);
    case BSR:  // branch and call
      return InsnDesc(InsnDesc::SUBR_REL);

      // Technically all these instructions are identical except for
      // hints.  We have to assume the compiler's hint is actually
      // what generally happens...
    case OpJump: // (Table 4-3)
      switch (insn & MBR_MASK)
	{
	case JMP:
	  return InsnDesc(InsnDesc::BR_UN_COND_IND);
	case JSR:
	case JSR_COROUTINE:  // FIXME: return and call
	  return InsnDesc(InsnDesc::SUBR_IND);
	case RET:
	  return InsnDesc(InsnDesc::SUBR_RET);
	}
      break;

      // ---------------------------------------------------
      // Integer operate Instructions (SS 4.4, 4.5, 4.6; Tables 4-5, 4-6, 4-7)
      // ---------------------------------------------------
    case OpIntArith:
      // (Mostly) SS 4.4, Table 4-5: Integer Arithmetic
      switch (insn & OPR_MASK)
	{
	case ADDL:		/* (SS 4.4, Table 4-5) */
	case ADDL_V:		/* (SS 4.4, Table 4-5) */
	case ADDQ:		/* (SS 4.4, Table 4-5) */
	case ADDQ_V:		/* (SS 4.4, Table 4-5) */
	case S4ADDL:		/* (SS 4.4, Table 4-5) */
	case S4ADDQ:		/* (SS 4.4, Table 4-5) */
	case S8ADDL:		/* (SS 4.4, Table 4-5) */
	case S8ADDQ:		/* (SS 4.4, Table 4-5) */
	  return InsnDesc(InsnDesc::INT_ADD); // Integer add
	case CMPEQ:		/* (SS 4.4, Table 4-5) */
	case CMPLT:		/* (SS 4.4, Table 4-5) */
	case CMPLE:		/* (SS 4.4, Table 4-5) */
	case CMPULT:		/* (SS 4.4, Table 4-5) */
	case CMPULE:		/* (SS 4.4, Table 4-5) */
	case CMPBGE:		/* (SS 4.6, Table 4-7) */
	  return InsnDesc(InsnDesc::INT_CMP); // Integer compare
	case SUBL:		/* (SS 4.4, Table 4-5) */
	case SUBL_V:		/* (SS 4.4, Table 4-5) */
	case SUBQ:		/* (SS 4.4, Table 4-5) */
	case SUBQ_V:		/* (SS 4.4, Table 4-5) */
	case S4SUBL:		/* (SS 4.4, Table 4-5) */
	case S4SUBQ:		/* (SS 4.4, Table 4-5) */
	case S8SUBL:		/* (SS 4.4, Table 4-5) */
	case S8SUBQ:		/* (SS 4.4, Table 4-5) */
	  return InsnDesc(InsnDesc::INT_SUB); // Integer subtract
	}
      break;

      // (Mostly) SS 4.5, Table 4-6: Logical and Shift Instructions
    case OpIntLogic:
      switch (insn & OPR_MASK)
	{
	case AND:		/* (SS 4.5, Table 4-6) */
	case BIC:		/* (SS 4.5, Table 4-6) */ /* ANDNOT */
	  return InsnDesc(InsnDesc::INT_LOGIC); // AND
	case BIS:		/* (SS 4.5, Table 4-6) */ /* OR */
	  // nop: ra=31,rb=31,rc=31
	case ORNOT:		/* (SS 4.5, Table 4-6) */
	  return InsnDesc(InsnDesc::INT_LOGIC); // OR
	case EQV:		/* (SS 4.5, Table 4-6) */ /* XORNOT */
	case XOR:		/* (SS 4.5, Table 4-6) */
	  return InsnDesc(InsnDesc::INT_LOGIC); // XOR
	case CMOVLBS:		/* (SS 4.5, Table 4-6) */
	case CMOVLBC:		/* (SS 4.5, Table 4-6) */
	case CMOVEQ:		/* (SS 4.5, Table 4-6) */
	case CMOVNE:		/* (SS 4.5, Table 4-6) */
	case CMOVLT:		/* (SS 4.5, Table 4-6) */
	case CMOVGE:		/* (SS 4.5, Table 4-6) */
	case CMOVLE:		/* (SS 4.5, Table 4-6) */
	case CMOVGT:		/* (SS 4.5, Table 4-6) */
	  return InsnDesc(InsnDesc::INT_MOV); // comparison and move
	case AMASK:		/* (SS 4.11, Table 4-17) */
	case IMPLVER:		/* (SS 4.11, Table 4-17) */
	  return InsnDesc(InsnDesc::OTHER);
	}
      break;

      // (Mostly) SS 4.6, Table 4-7: Byte Manipulation Instructions
    case OpIntShift:
      switch (insn & OPR_MASK)
	{
	case EXTBL:		/* (SS 4.6, Table 4-7) */
	case EXTWL:		/* (SS 4.6, Table 4-7) */
	case EXTLL:		/* (SS 4.6, Table 4-7) */
	case EXTQL:		/* (SS 4.6, Table 4-7) */
	case EXTWH:		/* (SS 4.6, Table 4-7) */
	case EXTLH:		/* (SS 4.6, Table 4-7) */
	case EXTQH:		/* (SS 4.6, Table 4-7) */
	case INSBL:		/* (SS 4.6, Table 4-7) */
	case INSWL:		/* (SS 4.6, Table 4-7) */
	case INSLL:		/* (SS 4.6, Table 4-7) */
	case INSQL:		/* (SS 4.6, Table 4-7) */
	case INSWH:		/* (SS 4.6, Table 4-7) */
	case INSLH:		/* (SS 4.6, Table 4-7) */
	case INSQH:		/* (SS 4.6, Table 4-7) */
	case MSKBL:		/* (SS 4.6, Table 4-7) */
	case MSKWL:		/* (SS 4.6, Table 4-7) */
	case MSKLL:		/* (SS 4.6, Table 4-7) */
	case MSKQL:		/* (SS 4.6, Table 4-7) */
	case MSKWH:		/* (SS 4.6, Table 4-7) */
	case MSKLH:		/* (SS 4.6, Table 4-7) */
	case MSKQH:		/* (SS 4.6, Table 4-7) */
	case ZAP:		/* (SS 4.6, Table 4-7) */
	case ZAPNOT:		/* (SS 4.6, Table 4-7) */
	  return InsnDesc(InsnDesc::INT_SHIFT);
	case SLL:		/* (SS 4.5, Table 4-6) */
	case SRA:		/* (SS 4.5, Table 4-6) */
	case SRL:		/* (SS 4.5, Table 4-6) */
	  return InsnDesc(InsnDesc::INT_SHIFT);
	}
      break;

    case OpIntMult:
      switch (insn & OPR_MASK)
	{
	case MULL:		/* (SS 4.4, Table 4-5) */
	case MULL_V:		/* (SS 4.4, Table 4-5) */
	case MULQ:		/* (SS 4.4, Table 4-5) */
	case MULQ_V:		/* (SS 4.4, Table 4-5) */
	case UMULH:		/* (SS 4.4, Table 4-5) */
	  return InsnDesc(InsnDesc::INT_MUL);
	}
      break;

      // ---------------------------------------------------
      // FP operate Instructions (SS 4.10, Table 4-16)
      // ---------------------------------------------------
    case OpIntToFlt:
       switch (insn & FP_MASK)
	 {
	 case ITOFF:		/* (SS 4.10, Table 4-16) */
	 case ITOFS:		/* (SS 4.10, Table 4-16) */
	 case ITOFT:		/* (SS 4.10, Table 4-16) */
	   return InsnDesc(InsnDesc::OTHER);
	 case SQRTF:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_C:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_UC:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_U:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_SC:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_S:		/* (SS 4.10, Table 4-16) */
	 case SQRTF_SUC:	/* (SS 4.10, Table 4-16) */
	 case SQRTF_SU:		/* (SS 4.10, Table 4-16) */
	 case SQRTG:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_C:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_UC:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_U:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_SC:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_S:		/* (SS 4.10, Table 4-16) */
	 case SQRTG_SUC:	/* (SS 4.10, Table 4-16) */
	 case SQRTG_SU:		/* (SS 4.10, Table 4-16) */
	 case SQRTS:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_C:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_M:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_D:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_UC:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_UM:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_U:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_UD:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUC:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUM:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SU:		/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUD:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUIC:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUIM:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUI:	/* (SS 4.10, Table 4-16) */
	 case SQRTS_SUID:	/* (SS 4.10, Table 4-16) */
	 case SQRTT:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_C:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_M:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_D:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_UC:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_UM:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_U:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_UD:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUC:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUM:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SU:		/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUD:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUIC:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUIM:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUI:	/* (SS 4.10, Table 4-16) */
	 case SQRTT_SUID:	/* (SS 4.10, Table 4-16) */
	   return InsnDesc(InsnDesc::FP_SQRT);
	 }
       break;

    case OpFltVAX:
      switch (insn & FP_MASK)
	{
	case ADDF:		/* (SS 4.10, Table 4-16) */
	case ADDF_C:		/* (SS 4.10, Table 4-16) */
	case ADDF_UC:		/* (SS 4.10, Table 4-16) */
	case ADDF_U:		/* (SS 4.10, Table 4-16) */
	case ADDF_SC:		/* (SS 4.10, Table 4-16) */
	case ADDF_S:		/* (SS 4.10, Table 4-16) */
	case ADDF_SUC:		/* (SS 4.10, Table 4-16) */
	case ADDF_SU:		/* (SS 4.10, Table 4-16) */
	case ADDG:		/* (SS 4.10, Table 4-16) */
	case ADDG_C:		/* (SS 4.10, Table 4-16) */
	case ADDG_UC:		/* (SS 4.10, Table 4-16) */
	case ADDG_U:		/* (SS 4.10, Table 4-16) */
	case ADDG_SC:		/* (SS 4.10, Table 4-16) */
	case ADDG_S:		/* (SS 4.10, Table 4-16) */
	case ADDG_SUC:		/* (SS 4.10, Table 4-16) */
	case ADDG_SU:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_ADD);
	case SUBF:		/* (SS 4.10, Table 4-16) */
	case SUBF_C:		/* (SS 4.10, Table 4-16) */
	case SUBF_UC:		/* (SS 4.10, Table 4-16) */
	case SUBF_U:		/* (SS 4.10, Table 4-16) */
	case SUBF_SC:		/* (SS 4.10, Table 4-16) */
	case SUBF_S:		/* (SS 4.10, Table 4-16) */
	case SUBF_SUC:		/* (SS 4.10, Table 4-16) */
	case SUBF_SU:		/* (SS 4.10, Table 4-16) */
	case SUBG:		/* (SS 4.10, Table 4-16) */
	case SUBG_C:		/* (SS 4.10, Table 4-16) */
	case SUBG_UC:		/* (SS 4.10, Table 4-16) */
	case SUBG_U:		/* (SS 4.10, Table 4-16) */
	case SUBG_SC:		/* (SS 4.10, Table 4-16) */
	case SUBG_S:		/* (SS 4.10, Table 4-16) */
	case SUBG_SUC:		/* (SS 4.10, Table 4-16) */
	case SUBG_SU:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_SUB);
	case CMPGEQ:		/* (SS 4.10, Table 4-16) */
	case CMPGLT:		/* (SS 4.10, Table 4-16) */
	case CMPGLE:		/* (SS 4.10, Table 4-16) */
	case CMPGEQ_S:		/* (SS 4.10, Table 4-16) */
	case CMPGLT_S:		/* (SS 4.10, Table 4-16) */
	case CMPGLE_S:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_CMP);
  	case CVTDG:		/* (SS 4.10, Table 4-16) */
	case CVTDG_C:		/* (SS 4.10, Table 4-16) */
	case CVTDG_UC:		/* (SS 4.10, Table 4-16) */
	case CVTDG_U:		/* (SS 4.10, Table 4-16) */
	case CVTDG_SC:		/* (SS 4.10, Table 4-16) */
	case CVTDG_S:		/* (SS 4.10, Table 4-16) */
	case CVTDG_SUC:		/* (SS 4.10, Table 4-16) */
	case CVTDG_SU:		/* (SS 4.10, Table 4-16) */
	case CVTGD:		/* (SS 4.10, Table 4-16) */
	case CVTGD_C:		/* (SS 4.10, Table 4-16) */
	case CVTGD_UC:		/* (SS 4.10, Table 4-16) */
	case CVTGD_U:		/* (SS 4.10, Table 4-16) */
	case CVTGD_SC:		/* (SS 4.10, Table 4-16) */
	case CVTGD_S:		/* (SS 4.10, Table 4-16) */
	case CVTGD_SUC:		/* (SS 4.10, Table 4-16) */
	case CVTGD_SU:		/* (SS 4.10, Table 4-16) */
	case CVTGF:		/* (SS 4.10, Table 4-16) */
	case CVTGF_C:		/* (SS 4.10, Table 4-16) */
	case CVTGF_UC:		/* (SS 4.10, Table 4-16) */
	case CVTGF_U:		/* (SS 4.10, Table 4-16) */
	case CVTGF_SC:		/* (SS 4.10, Table 4-16) */
	case CVTGF_S:		/* (SS 4.10, Table 4-16) */
	case CVTGF_SUC:		/* (SS 4.10, Table 4-16) */
	case CVTGF_SU:		/* (SS 4.10, Table 4-16) */
	case CVTGQ:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_C:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_VC:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_V:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_SC:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_S:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_SVC:		/* (SS 4.10, Table 4-16) */
	case CVTGQ_SV:		/* (SS 4.10, Table 4-16) */
	case CVTQF:		/* (SS 4.10, Table 4-16) */
	case CVTQF_C:		/* (SS 4.10, Table 4-16) */
	case CVTQG:		/* (SS 4.10, Table 4-16) */
	case CVTQG_C:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_CVT);
	case DIVF:		/* (SS 4.10, Table 4-16) */
	case DIVF_C:		/* (SS 4.10, Table 4-16) */
	case DIVF_UC:		/* (SS 4.10, Table 4-16) */
	case DIVF_U:		/* (SS 4.10, Table 4-16) */
	case DIVF_SC:		/* (SS 4.10, Table 4-16) */
	case DIVF_S:		/* (SS 4.10, Table 4-16) */
	case DIVF_SUC:		/* (SS 4.10, Table 4-16) */
	case DIVF_SU:		/* (SS 4.10, Table 4-16) */
	case DIVG:		/* (SS 4.10, Table 4-16) */
	case DIVG_C:		/* (SS 4.10, Table 4-16) */
	case DIVG_UC:		/* (SS 4.10, Table 4-16) */
	case DIVG_U:		/* (SS 4.10, Table 4-16) */
	case DIVG_SC:		/* (SS 4.10, Table 4-16) */
	case DIVG_S:		/* (SS 4.10, Table 4-16) */
	case DIVG_SUC:		/* (SS 4.10, Table 4-16) */
	case DIVG_SU:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_DIV);
	case MULF:		/* (SS 4.10, Table 4-16) */
	case MULF_C:		/* (SS 4.10, Table 4-16) */
	case MULF_UC:		/* (SS 4.10, Table 4-16) */
	case MULF_U:		/* (SS 4.10, Table 4-16) */
	case MULF_SC:		/* (SS 4.10, Table 4-16) */
	case MULF_S:		/* (SS 4.10, Table 4-16) */
	case MULF_SUC:		/* (SS 4.10, Table 4-16) */
	case MULF_SU:		/* (SS 4.10, Table 4-16) */
	case MULG:		/* (SS 4.10, Table 4-16) */
	case MULG_C:		/* (SS 4.10, Table 4-16) */
	case MULG_UC:		/* (SS 4.10, Table 4-16) */
	case MULG_U:		/* (SS 4.10, Table 4-16) */
	case MULG_SC:		/* (SS 4.10, Table 4-16) */
	case MULG_S:		/* (SS 4.10, Table 4-16) */
	case MULG_SUC:		/* (SS 4.10, Table 4-16) */
	case MULG_SU:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_MUL);
	}
      break;

    case OpFltIEEE:
      switch (insn & FP_MASK)
	{
	case ADDS:		/* (SS 4.10, Table 4-16) */
	case ADDS_C:		/* (SS 4.10, Table 4-16) */
	case ADDS_M:		/* (SS 4.10, Table 4-16) */
	case ADDS_D:		/* (SS 4.10, Table 4-16) */
	case ADDS_UC:		/* (SS 4.10, Table 4-16) */
	case ADDS_UM:		/* (SS 4.10, Table 4-16) */
	case ADDS_U:		/* (SS 4.10, Table 4-16) */
	case ADDS_UD:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUC:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUM:		/* (SS 4.10, Table 4-16) */
	case ADDS_SU:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUD:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUIC:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUIM:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUI:		/* (SS 4.10, Table 4-16) */
	case ADDS_SUID:		/* (SS 4.10, Table 4-16) */
	case ADDT:		/* (SS 4.10, Table 4-16) */
	case ADDT_C:		/* (SS 4.10, Table 4-16) */
	case ADDT_M:		/* (SS 4.10, Table 4-16) */
	case ADDT_D:		/* (SS 4.10, Table 4-16) */
	case ADDT_UC:		/* (SS 4.10, Table 4-16) */
	case ADDT_UM:		/* (SS 4.10, Table 4-16) */
	case ADDT_U:		/* (SS 4.10, Table 4-16) */
	case ADDT_UD:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUC:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUM:		/* (SS 4.10, Table 4-16) */
	case ADDT_SU:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUD:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUIC:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUIM:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUI:		/* (SS 4.10, Table 4-16) */
	case ADDT_SUID:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_ADD);
 	case CMPTUN:		/* (SS 4.10, Table 4-16) */
	case CMPTEQ:		/* (SS 4.10, Table 4-16) */
	case CMPTLT:		/* (SS 4.10, Table 4-16) */
	case CMPTLE:		/* (SS 4.10, Table 4-16) */
	case CMPTUN_SU:		/* (SS 4.10, Table 4-16) */
	case CMPTEQ_SU:		/* (SS 4.10, Table 4-16) */
	case CMPTLT_SU:		/* (SS 4.10, Table 4-16) */
	case CMPTLE_SU:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_CMP);
	case CVTQS:		/* (SS 4.10, Table 4-16) */
	case CVTQS_C:		/* (SS 4.10, Table 4-16) */
	case CVTQS_M:		/* (SS 4.10, Table 4-16) */
	case CVTQS_D:		/* (SS 4.10, Table 4-16) */
	case CVTQS_SUIC:	/* (SS 4.10, Table 4-16) */
	case CVTQS_SUIM:	/* (SS 4.10, Table 4-16) */
	case CVTQS_SUI:		/* (SS 4.10, Table 4-16) */
	case CVTQS_SUID:	/* (SS 4.10, Table 4-16) */
	case CVTQT:		/* (SS 4.10, Table 4-16) */
	case CVTQT_C:		/* (SS 4.10, Table 4-16) */
	case CVTQT_M:		/* (SS 4.10, Table 4-16) */
	case CVTQT_D:		/* (SS 4.10, Table 4-16) */
	case CVTQT_SUIC:	/* (SS 4.10, Table 4-16) */
	case CVTQT_SUIM:	/* (SS 4.10, Table 4-16) */
	case CVTQT_SUI:		/* (SS 4.10, Table 4-16) */
	case CVTQT_SUID:	/* (SS 4.10, Table 4-16) */
	case CVTST:		/* (SS 4.10, Table 4-16) */
	case CVTST_S:		/* (SS 4.10, Table 4-16) */
	case CVTTQ:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_C:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_M:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_D:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_VC:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_VM:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_V:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_VD:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVC:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVM:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SV:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVD:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVIC:	/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVIM:	/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVI:		/* (SS 4.10, Table 4-16) */
	case CVTTQ_SVID:	/* (SS 4.10, Table 4-16) */
	case CVTTS:		/* (SS 4.10, Table 4-16) */
	case CVTTS_C:		/* (SS 4.10, Table 4-16) */
	case CVTTS_M:		/* (SS 4.10, Table 4-16) */
	case CVTTS_D:		/* (SS 4.10, Table 4-16) */
	case CVTTS_UC:		/* (SS 4.10, Table 4-16) */
	case CVTTS_UM:		/* (SS 4.10, Table 4-16) */
	case CVTTS_U:		/* (SS 4.10, Table 4-16) */
	case CVTTS_UD:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SUC:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SUM:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SU:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SUD:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SUIC:	/* (SS 4.10, Table 4-16) */
	case CVTTS_SUIM:	/* (SS 4.10, Table 4-16) */
	case CVTTS_SUI:		/* (SS 4.10, Table 4-16) */
	case CVTTS_SUID:	/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_CVT);
	case DIVS:		/* (SS 4.10, Table 4-16) */
	case DIVS_C:		/* (SS 4.10, Table 4-16) */
	case DIVS_M:		/* (SS 4.10, Table 4-16) */
	case DIVS_D:		/* (SS 4.10, Table 4-16) */
	case DIVS_UC:		/* (SS 4.10, Table 4-16) */
	case DIVS_UM:		/* (SS 4.10, Table 4-16) */
	case DIVS_U:		/* (SS 4.10, Table 4-16) */
	case DIVS_UD:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUC:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUM:		/* (SS 4.10, Table 4-16) */
	case DIVS_SU:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUD:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUIC:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUIM:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUI:		/* (SS 4.10, Table 4-16) */
	case DIVS_SUID:		/* (SS 4.10, Table 4-16) */
	case DIVT:		/* (SS 4.10, Table 4-16) */
	case DIVT_C:		/* (SS 4.10, Table 4-16) */
	case DIVT_M:		/* (SS 4.10, Table 4-16) */
	case DIVT_D:		/* (SS 4.10, Table 4-16) */
	case DIVT_UC:		/* (SS 4.10, Table 4-16) */
	case DIVT_UM:		/* (SS 4.10, Table 4-16) */
	case DIVT_U:		/* (SS 4.10, Table 4-16) */
	case DIVT_UD:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUC:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUM:		/* (SS 4.10, Table 4-16) */
	case DIVT_SU:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUD:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUIC:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUIM:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUI:		/* (SS 4.10, Table 4-16) */
	case DIVT_SUID:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_DIV);
	case MULS:		/* (SS 4.10, Table 4-16) */
	case MULS_C:		/* (SS 4.10, Table 4-16) */
	case MULS_M:		/* (SS 4.10, Table 4-16) */
	case MULS_D:		/* (SS 4.10, Table 4-16) */
	case MULS_UC:		/* (SS 4.10, Table 4-16) */
	case MULS_UM:		/* (SS 4.10, Table 4-16) */
	case MULS_U:		/* (SS 4.10, Table 4-16) */
	case MULS_UD:		/* (SS 4.10, Table 4-16) */
	case MULS_SUC:		/* (SS 4.10, Table 4-16) */
	case MULS_SUM:		/* (SS 4.10, Table 4-16) */
	case MULS_SU:		/* (SS 4.10, Table 4-16) */
	case MULS_SUD:		/* (SS 4.10, Table 4-16) */
	case MULS_SUIC:		/* (SS 4.10, Table 4-16) */
	case MULS_SUIM:		/* (SS 4.10, Table 4-16) */
	case MULS_SUI:		/* (SS 4.10, Table 4-16) */
	case MULS_SUID:		/* (SS 4.10, Table 4-16) */
	case MULT:		/* (SS 4.10, Table 4-16) */
	case MULT_C:		/* (SS 4.10, Table 4-16) */
	case MULT_M:		/* (SS 4.10, Table 4-16) */
	case MULT_D:		/* (SS 4.10, Table 4-16) */
	case MULT_UC:		/* (SS 4.10, Table 4-16) */
	case MULT_UM:		/* (SS 4.10, Table 4-16) */
	case MULT_U:		/* (SS 4.10, Table 4-16) */
	case MULT_UD:		/* (SS 4.10, Table 4-16) */
	case MULT_SUC:		/* (SS 4.10, Table 4-16) */
	case MULT_SUM:		/* (SS 4.10, Table 4-16) */
	case MULT_SU:		/* (SS 4.10, Table 4-16) */
	case MULT_SUD:		/* (SS 4.10, Table 4-16) */
	case MULT_SUIC:		/* (SS 4.10, Table 4-16) */
	case MULT_SUIM:		/* (SS 4.10, Table 4-16) */
	case MULT_SUI:		/* (SS 4.10, Table 4-16) */
	case MULT_SUID:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_MUL);
	case SUBS:		/* (SS 4.10, Table 4-16) */
	case SUBS_C:		/* (SS 4.10, Table 4-16) */
	case SUBS_M:		/* (SS 4.10, Table 4-16) */
	case SUBS_D:		/* (SS 4.10, Table 4-16) */
	case SUBS_UC:		/* (SS 4.10, Table 4-16) */
	case SUBS_UM:		/* (SS 4.10, Table 4-16) */
	case SUBS_U:		/* (SS 4.10, Table 4-16) */
	case SUBS_UD:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUC:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUM:		/* (SS 4.10, Table 4-16) */
	case SUBS_SU:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUD:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUIC:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUIM:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUI:		/* (SS 4.10, Table 4-16) */
	case SUBS_SUID:		/* (SS 4.10, Table 4-16) */
	case SUBT:		/* (SS 4.10, Table 4-16) */
	case SUBT_C:		/* (SS 4.10, Table 4-16) */
	case SUBT_M:		/* (SS 4.10, Table 4-16) */
	case SUBT_D:		/* (SS 4.10, Table 4-16) */
	case SUBT_UC:		/* (SS 4.10, Table 4-16) */
	case SUBT_UM:		/* (SS 4.10, Table 4-16) */
	case SUBT_U:		/* (SS 4.10, Table 4-16) */
	case SUBT_UD:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUC:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUM:		/* (SS 4.10, Table 4-16) */
	case SUBT_SU:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUD:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUIC:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUIM:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUI:		/* (SS 4.10, Table 4-16) */
	case SUBT_SUID:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_SUB);
	}
      break;

    case OpFltOp:
      switch (insn & FP_MASK)
	{
	case CPYS:		/* (SS 4.10, Table 4-16) */
	case CPYSE:		/* (SS 4.10, Table 4-16) */
	case CPYSN:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_OTHER);
	case CVTLQ:		/* (SS 4.10, Table 4-16) */
	case CVTQL:		/* (SS 4.10, Table 4-16) */
	case CVTQL_V:		/* (SS 4.10, Table 4-16) */
	case CVTQL_SV:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_CVT);
	case FCMOVEQ:		/* (SS 4.10, Table 4-16) */
	case FCMOVNE:		/* (SS 4.10, Table 4-16) */
	case FCMOVLT:		/* (SS 4.10, Table 4-16) */
	case FCMOVGE:		/* (SS 4.10, Table 4-16) */
	case FCMOVLE:		/* (SS 4.10, Table 4-16) */
	case FCMOVGT:		/* (SS 4.10, Table 4-16) */
	case MF_FPCR:		/* (SS 4.10, Table 4-16) */
	case MT_FPCR:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::FP_MOV);
	}
      break;

      // ---------------------------------------------------
      // Other/Misc Instructions (SS 4.11 & 4.12, Table 4-17, 4-18)
      // ---------------------------------------------------
    case OpMisc:
      switch (insn & MFC_MASK)
	{
	  // Miscellaneous Instructions: (SS 4.11, Table 4-17)
	case ECB:		/* (SS 4.11, Table 4-17) */
	case EXCB:		/* (SS 4.11, Table 4-17) */
	case FETCH:		/* (SS 4.11, Table 4-17) */
	case FETCH_M:		/* (SS 4.11, Table 4-17) */
	case MB:		/* (SS 4.11, Table 4-17) */
	case RPCC:		/* (SS 4.11, Table 4-17) */
	case TRAPB:		/* (SS 4.11, Table 4-17) */
	case WH64:		/* (SS 4.11, Table 4-17) */
	case WMB:		/* (SS 4.11, Table 4-17) */
	  return InsnDesc(InsnDesc::OTHER);

	  // VAX compatibility Instructions: (SS 4.12, Table 4-18)
	case RC:		/* (SS 4.12, Table 4-18) */
	case RS:		/* (SS 4.12, Table 4-18) */
	  return InsnDesc(InsnDesc::OTHER);
	}
      break;

      // ---------------------------------------------------
      // (Mostly) Multimedia Instructions: (SS 4.13, Table 4-19(?))
      // ---------------------------------------------------
    case OpFltToInt:
      switch (insn & FP_MASK)
	{
	case MINUB8:		/* (SS 4.13, Table 4-19(?)) */
	case MINSB8:		/* (SS 4.13, Table 4-19(?)) */
	case MINUW4:		/* (SS 4.13, Table 4-19(?)) */
	case MINSW4:		/* (SS 4.13, Table 4-19(?)) */
	case MAXUB8:		/* (SS 4.13, Table 4-19(?)) */
	case MAXSB8:		/* (SS 4.13, Table 4-19(?)) */
	case MAXUW4:		/* (SS 4.13, Table 4-19(?)) */
	case MAXSW4:		/* (SS 4.13, Table 4-19(?)) */
	case PERR:		/* (SS 4.13, Table 4-19(?)) */
	case PKLB:		/* (SS 4.13, Table 4-19(?)) */
	case PKWB:		/* (SS 4.13, Table 4-19(?)) */
	case UNPKBL:		/* (SS 4.13, Table 4-19(?)) */
	case UNPKBW:		/* (SS 4.13, Table 4-19(?)) */
	  return InsnDesc(InsnDesc::INT_OTHER);
	case CTLZ:		/* (SS 4.4, Table 4-5) */
	case CTPOP:		/* (SS 4.4, Table 4-5) */
	case CTTZ:		/* (SS 4.4, Table 4-5) */
	  return InsnDesc(InsnDesc::INT_OTHER);
	case SEXTB:		/* (SS 4.6, Table 4-7) */
	case SEXTW:		/* (SS 4.6, Table 4-7) */
	  return InsnDesc(InsnDesc::INT_OTHER);
	case FTOIS:		/* (SS 4.10, Table 4-16) */
	case FTOIT:		/* (SS 4.10, Table 4-16) */
	  return InsnDesc(InsnDesc::OTHER);
	}
      break;

      // ---------------------------------------------------
      // Generic PALcode format instructions
      // ---------------------------------------------------
    case CALL_PAL:
    case PAL19:
    // case PAL1B: // already referenced as HW_LD
    case PAL1D:
    case PAL1E:
    // case PAL1F: // already referenced as HW_ST
      return InsnDesc(InsnDesc::OTHER);

      // PALcode format instructions
    case CALLSYS:
      return InsnDesc(InsnDesc::SYS_CALL);

    default:
      break;
    }

  return InsnDesc(InsnDesc::INVALID);
}


VMA
AlphaISA::getInsnTargetVMA(MachInsn* mi, VMA pc, ushort GCC_ATTR_UNUSED opIndex,
			   ushort GCC_ATTR_UNUSED sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t insn = (uint32_t)bfd_getl32((const unsigned char*)mi);

  intptr_t offset;
  switch (insn & OP_MASK)
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
      offset = BR_DISP(insn);
      if (offset & BR_DISP_SIGN) { offset |= ~BR_DISP_MASK; } // sign extend
      return ((pc + MINSN_SIZE) + (offset << 2));

    case OpJump: // JMP, JSR, JSR_COROUTINE, RET
      break;  // branch content in register

    default:
      break;
    }

  return (0);
}


void
AlphaISA::decode(ostream& GCC_ATTR_UNUSED os, MachInsn* GCC_ATTR_UNUSED mi,
		 VMA GCC_ATTR_UNUSED vma, ushort GCC_ATTR_UNUSED opIndex)
{
  DIAG_Die(DIAG_Unimplemented);
}
