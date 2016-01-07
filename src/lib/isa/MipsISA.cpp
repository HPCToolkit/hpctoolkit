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

#include <include/uint.h>
#include <include/gnu_bfd.h>  // for bfd_getb32, bfd_getl32
#include <include/gnu_dis-asm.h>

#include "MipsISA.hpp"

#include <lib/isa-lean/mips/instruction-set.h>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

#if defined(HOST_PLATFORM_MIPS64LE_LINUX)
# define BFD_GETX32 bfd_getl32
# define BFD_PRINT_INSN_MIPS print_insn_little_mips
#else /* PLATFORM_MIPS_IRIX64 */
# define BFD_GETX32 bfd_getb32
# define BFD_PRINT_INSN_MIPS print_insn_big_mips
#endif

//****************************************************************************

static VMA
GNUvma2vma(bfd_vma di_vma, MachInsn* insn_addr, VMA insn_vma)
{
  // N.B.: The GNU decoders assume that the address of 'insn_addr' is
  // the actual the VMA in order to calculate VMA-relative targets.
  VMA x = (di_vma - PTR_TO_BFDVMA(insn_addr)) + insn_vma;
  return x;
}


static void
GNUbu_print_addr(bfd_vma di_vma, struct disassemble_info* di)
{
  GNUbu_disdata* data = (GNUbu_disdata*)di->application_data;

  VMA x = GNUvma2vma(di_vma, data->insn_addr, data->insn_vma);
  ostream* os = (ostream*)di->stream;
  *os << std::showbase << std::hex << x << std::dec;
}

//****************************************************************************
// MipsISA
//****************************************************************************

// See 'ISA.h' for comments on the interface

MipsISA::MipsISA()
  : m_di(NULL), m_di_dis(NULL)
{
  // See 'dis-asm.h'
  m_di = new disassemble_info;
  init_disassemble_info(m_di, stdout, GNUbu_fprintf_stub);
  m_di->arch = bfd_arch_mips;        // bfd_get_arch(abfd)
  m_di->mach = bfd_mach_mipsisa64r2; // bfd_get_mach(abfd)
#if defined(HOST_PLATFORM_MIPS64LE_LINUX)
  m_di->endian = BFD_ENDIAN_LITTLE;
#else
  m_di->endian = BFD_ENDIAN_BIG;
#endif
  m_di->read_memory_func = GNUbu_read_memory; // vs. 'buffer_read_memory'
  m_di->print_address_func = GNUbu_print_addr_stub; // vs. 'generic_print_addr'

  m_di_dis = new disassemble_info;
  init_disassemble_info(m_di_dis, stdout, GNUbu_fprintf);
  m_di_dis->application_data = (void*)&m_dis_data;
  m_di_dis->arch = m_di->arch;
  m_di_dis->mach = m_di->mach;
  m_di_dis->endian = m_di->endian;
  m_di_dis->read_memory_func = GNUbu_read_memory;
  m_di_dis->print_address_func = GNUbu_print_addr;
}


MipsISA::~MipsISA()
{
  delete m_di;
  delete m_di_dis;
}


ISA::InsnDesc
MipsISA::getInsnDesc(MachInsn* mi, ushort GCC_ATTR_UNUSED opIndex,
		     ushort GCC_ATTR_UNUSED sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t insn = (uint32_t)BFD_GETX32((const unsigned char*)mi);

  switch (insn & MIPS_OPCODE_MASK)
    {
    case MIPS_OPClass_Special:
      switch (insn & MIPS_OPClass_Special_MASK)
	{
	case MIPS_OP_JR:                // Instructions from Table A-13
	  // JR $31 returns from a JAL call instruction.
	  if (MIPS_OPND_REG_S(insn) == MIPS_REG_RA) {
	    return InsnDesc(InsnDesc::SUBR_RET);
	  }
	  else {
	    return InsnDesc(InsnDesc::BR_UN_COND_IND);
	  }
	case MIPS_OP_JALR:
	  return InsnDesc(InsnDesc::SUBR_IND);
	
	case MIPS_OP_SYSCALL:           // Instructions from Table A-16,
	case MIPS_OP_BREAK:             //   Table A-17
	case MIPS_OP_TGE:   // Trap-on-Condition...
	case MIPS_OP_TGEU:
	case MIPS_OP_TLT:
	case MIPS_OP_TLTU:
	case MIPS_OP_TEQ:
	case MIPS_OP_TNE:
	  return InsnDesc(InsnDesc::SYS_CALL);

	case MIPS_OP_SYNC:
	  return InsnDesc(InsnDesc::OTHER);
	}
      break;
    case MIPS_OPClass_RegImm:
      switch (insn & MIPS_OPClass_RegImm_MASK)
	{
	case MIPS_OP_BLTZ:              // Instructions from Table A-15
	case MIPS_OP_BGEZ:
	case MIPS_OP_BLTZL:
	case MIPS_OP_BGEZL:
	  return InsnDesc(InsnDesc::INT_BR_COND_REL);
	
	case MIPS_OP_BLTZAL:  // Link
	case MIPS_OP_BGEZAL:  // Link
	case MIPS_OP_BLTZALL: // Link
	case MIPS_OP_BGEZALL: // Link
	  return InsnDesc(InsnDesc::SUBR_REL);

	case MIPS_OP_TGEI:              // Instructions from Table A-18
	case MIPS_OP_TGEIU: // Trap-on-Condition...
	case MIPS_OP_TLTI:
	case MIPS_OP_TLTIU:
	case MIPS_OP_TEQI:
	case MIPS_OP_TNEI:
	  return InsnDesc(InsnDesc::OTHER);
	}
      break;
    case MIPS_OPClass_Cop1x:
      switch (insn & MIPS_OPClass_Cop1x_MASK)
	{
	case MIPS_OP_LWXC1: // Word	// Instructions from Table A-7
	  return InsnDesc(InsnDesc::MEM_LOAD);
	case MIPS_OP_LDXC1: // Doubleword
	  return InsnDesc(InsnDesc::MEM_LOAD);
	case MIPS_OP_SWXC1: // Word
	  return InsnDesc(InsnDesc::MEM_STORE);
	case MIPS_OP_SDXC1: // Doubleword
	  return InsnDesc(InsnDesc::MEM_STORE);
	}
      break;

    // Normal instruction class (OPNormal)
    case MIPS_OP_LB:   // Byte          // Instructions from Table A-3
    case MIPS_OP_LBU:  // Byte Unsigned //  Table A-4, Table A-5, Table A-6
    case MIPS_OP_LH:   // Halfword
    case MIPS_OP_LHU:  // Halfword Unsigned
    case MIPS_OP_LW:   // Word
    case MIPS_OP_LWU:  // Word Unsigned
    case MIPS_OP_LWL:  // Word Left
    case MIPS_OP_LWR:  // Word Right
    case MIPS_OP_LL:   // Linked Word
    case MIPS_OP_LWC1: // Word
    case MIPS_OP_LWC2: // Word
      return InsnDesc(InsnDesc::MEM_LOAD);

    case MIPS_OP_LD:   // Doubleword
    case MIPS_OP_LDL:  // Doubleword Left
    case MIPS_OP_LDR:  // Doubleword Right
    case MIPS_OP_LLD:  // Linked Doubleword
    case MIPS_OP_LDC1: // Doubleword
    case MIPS_OP_LDC2: // Doubleword
      return InsnDesc(InsnDesc::MEM_LOAD); // Doubleword

    case MIPS_OP_SB:   // Byte
    case MIPS_OP_SH:   // Halfword
    case MIPS_OP_SW:   // Word
    case MIPS_OP_SWL:  // Word Left
    case MIPS_OP_SWR:  // Word Right
    case MIPS_OP_SC:   // Conditional Word
    case MIPS_OP_SWC1: // Word
    case MIPS_OP_SWC2: // Word
      return InsnDesc(InsnDesc::MEM_STORE);

    case MIPS_OP_SD:   // Doubleword
    case MIPS_OP_SDL:  // Doubleword Left
    case MIPS_OP_SDR:  // Doubleword Right
    case MIPS_OP_SCD:  // Conditional Doubleword
    case MIPS_OP_SDC1: // Doubleword
    case MIPS_OP_SDC2: // Doubleword
      return InsnDesc(InsnDesc::MEM_STORE); // Doubleword

                                // Instructions from Table A-12
      // N.B.: These instructions are PC-region but we will treat them
      // as PC-relative
    case MIPS_OP_J:
      return InsnDesc(InsnDesc::BR_UN_COND_REL);
    case MIPS_OP_JAL:
      return InsnDesc(InsnDesc::SUBR_REL);

    case MIPS_OP_BEQ:                   // Instructions from Table A-14
      // If operands are the same then then there is no fall-thru branch.
      // Test for general case.
      if (MIPS_OPND_REG_S(insn) == MIPS_OPND_REG_T(insn))
	return InsnDesc(InsnDesc::BR_UN_COND_REL);
      else
	return InsnDesc(InsnDesc::INT_BR_COND_REL);
    case MIPS_OP_BNE:
    case MIPS_OP_BLEZ:
    case MIPS_OP_BGTZ:
    case MIPS_OP_BEQL:
    case MIPS_OP_BNEL:
    case MIPS_OP_BLEZL:
    case MIPS_OP_BGTZL:
      return InsnDesc(InsnDesc::INT_BR_COND_REL);

    default:
      break;
    }

  return InsnDesc(InsnDesc::OTHER);
}


VMA
MipsISA::getInsnTargetVMA(MachInsn* mi, VMA pc, ushort GCC_ATTR_UNUSED opIndex,
			  ushort GCC_ATTR_UNUSED sz)
{
  // We know that instruction sizes are guaranteed to be 4 bytes, but
  // the host may have a different byte order than the executable.
  uint32_t insn = (uint32_t)BFD_GETX32((const unsigned char*)mi);

  intptr_t offset;
  switch (insn & MIPS_OPCODE_MASK)
    {
    case MIPS_OPClass_Special:
      switch (insn & MIPS_OPClass_Special_MASK)
	{
	case MIPS_OP_JR:                // Instructions from Table A-13
	case MIPS_OP_JALR:
	  break;  // branch content in register
	}
      break;
    case MIPS_OPClass_RegImm:
      switch (insn & MIPS_OPClass_RegImm_MASK)
	{
	case MIPS_OP_BLTZ:              // Instructions from Table A-15
	case MIPS_OP_BGEZ:
	case MIPS_OP_BLTZAL:
	case MIPS_OP_BGEZAL:
	case MIPS_OP_BLTZL:
	case MIPS_OP_BGEZL:
	case MIPS_OP_BLTZALL:
	case MIPS_OP_BGEZALL:
	  // Added to the address of the instruction *following* the branch
	  offset = MIPS_OPND_IMM(insn);
	  if (offset & MIPS_OPND_IMM_SIGN) {
	    offset |= ~MIPS_OPND_IMM_MASK;  // sign extend
	  }
	  return ((pc + MINSN_SIZE) + (offset << 2));
	}
      break;

    // Normal instruction class
    case MIPS_OP_J:                     // Instructions from Table A-12
    case MIPS_OP_JAL:
      // These are PC-Region and not PC-Relative instructions.
      // Upper order bits come from the address of the delay-slot instruction
      offset = MIPS_OPND_INSN_INDEX(insn) << 2;
      return (((pc + MINSN_SIZE) & MIPS_OPND_INSN_INDEX_UPPER_MASK) | offset);

    case MIPS_OP_BEQ:                   // Instructions from Table A-14
    case MIPS_OP_BNE:
    case MIPS_OP_BLEZ:
    case MIPS_OP_BGTZ:
    case MIPS_OP_BEQL:
    case MIPS_OP_BNEL:
    case MIPS_OP_BLEZL:
    case MIPS_OP_BGTZL:
      // Added to the address of the instruction *following* the branch
      offset = MIPS_OPND_IMM(insn);
      if (offset & MIPS_OPND_IMM_SIGN) {
	offset |= ~MIPS_OPND_IMM_MASK; // sign extend
      }
      return ((pc + MINSN_SIZE) + (offset << 2));

    default:
      break;
    }

  return (0);
}


ushort
MipsISA::getInsnNumDelaySlots(MachInsn* mi, ushort opIndex, ushort sz)
{
  // All branches have an architectural delay of one
  // instruction. Treat branch-likely instructions as regular
  // branches.

  // (We don't care about delays on instructions such as loads/stores)
  InsnDesc d = getInsnDesc(mi, opIndex, sz);
  if (d.isBr() || d.isSubr() || d.isSubrRet()) {
    return 1;
  }
  else {
    return 0;
  }
}


void
MipsISA::decode(ostream& os, MachInsn* mi, VMA vma,
		ushort GCC_ATTR_UNUSED opIndex)
{
  m_dis_data.insn_addr = mi;
  m_dis_data.insn_vma = vma;

  m_di_dis->stream = (void*)&os;
  BFD_PRINT_INSN_MIPS(PTR_TO_BFDVMA(mi), m_di_dis);
}


//****************************************************************************
