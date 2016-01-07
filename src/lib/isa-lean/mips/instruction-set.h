// -*-Mode: C++;-*- // technically C99

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
//    mips.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef isa_instructionset_mips_h
#define isa_instructionset_mips_h


// Information about MIPS instructions, encoding, conventions, and
// section numbers are from "MIPS IV Instruction Set," Revision 3.2,
// By Charles Price, September, 1995 MIPS Technologies, Inc.


//***************************************************************************
// Opcode Classes and Masks
//***************************************************************************

// Instruction Formats: See A-7 CPU Instruction Formats
//                       31 ............................................... 0
//   I-Type (Immediate): opcode (6), rs (5), rt (5), offset (16)
//   J-Type (Jump):	 opcode (6), instr_index (26)
//   R-Type (Register):  opcode (6), rs (5), rt (5), rd (5), sa (5), func (6)

// Instruction Classes:
//   OPNormal  - Usually, an instruction that has an immediate value or offset.
//   OPSpecial - 3-register computational instructions, jump register,
//     and some special purpose instructions. 
//   OPRegImm  - encodes conditional branch and trap immediate instructions.
//   OPCop0, OPcop1, OPcop2, OPCop1x (Cf. A-8.3.1 -- Cf. A-8.3.3) - FP
//     instructions
// 
// See A-8.1 for more information on decoding CPU instructions.
//   (B-12.1 gives information on decoding FPU instructions.  With
//   the exception of FPU load/stores, these instructions are subsumed
//   under the appropriate class-instruction.)

// Masks for the different classes.  The mask places a 1-bit at every
//   location used to decode an instruction.  AND the mask with the
//   instruction to find the opcode 
#define MIPS_OPCODE_MASK          0xfc000000  /* opcode: bits 31-26 */
#define MIPS_OPClass_Special_MASK 0xfc00003f  /* opcode + format bits: 5-0 */
#define MIPS_OPClass_RegImm_MASK  0xfc1f0000  /* opcode + rt bits: 20-16 */
#define MIPS_OPClass_Cop1x_MASK   0xfc00003f  /* opcode + function bits: 5-0 */

//   special purpose mask: cop1 insn's that modify CPU registers
#define MIPS_OPCop1_mv2cpu_MASK 0xffe007ff /* opcode + bits 25-21 + zeroed 10-0 */

// Opcode classes
#define MIPS_OPClass_Special    0x00000000     /* opcode for Special class */
#define MIPS_OPClass_RegImm     0x04000000     /* opcode for RegImm class */
#define MIPS_OPClass_Cop0       0x40000000     /* opcode for Cop0 class */
#define MIPS_OPClass_Cop1       0x44000000     /* opcode for Cop1 class */
#define MIPS_OPClass_Cop2       0x48000000     /* opcode for Cop2 class */
#define MIPS_OPClass_Cop1x      0x4c000000     /* opcode for Cop1x class */


//***************************************************************************
// Opcodes
//***************************************************************************

// After each instruction follows its class and format, respectively
  
// ---------------------------------------------------------
// Load and Store
// ---------------------------------------------------------

// Table A-3: Normal CPU Load/Store Instructions
#define MIPS_OP_LB	0x80000000  /* Normal ; Immediate */
#define MIPS_OP_LBU	0x90000000  /* Normal ; Immediate */
#define MIPS_OP_SB	0xa0000000  /* Normal ; Immediate */
#define MIPS_OP_LH	0x84000000  /* Normal ; Immediate */
#define MIPS_OP_LHU	0x94000000  /* Normal ; Immediate */
#define MIPS_OP_SH	0xa4000000  /* Normal ; Immediate */
#define MIPS_OP_LW	0x8c000000  /* Normal ; Immediate */
#define MIPS_OP_LWU	0x9c000000  /* Normal ; Immediate */
#define MIPS_OP_SW	0xac000000  /* Normal ; Immediate */
#define MIPS_OP_LD	0xdc000000  /* Normal ; Immediate */
#define MIPS_OP_SD	0xfc000000  /* Normal ; Immediate */

// Table A-4: Unaligned CPU Load/Store Instructions
#define MIPS_OP_LWL	0x88000000  /* Normal ; Immediate */
#define MIPS_OP_LWR	0x98000000  /* Normal ; Immediate */
#define MIPS_OP_SWL	0xa8000000  /* Normal ; Immediate */
#define MIPS_OP_SWR	0xb8000000  /* Normal ; Immediate */
#define MIPS_OP_LDL	0x68000000  /* Normal ; Immediate */
#define MIPS_OP_LDR	0x6c000000  /* Normal ; Immediate */
#define MIPS_OP_SDL	0xb0000000  /* Normal ; Immediate */
#define MIPS_OP_SDR	0xb4000000  /* Normal ; Immediate */

// Table A-5: Atomic Update CPU Load/Store Instructions
#define MIPS_OP_LL	0xc0000000  /* Normal ; Immediate */
#define MIPS_OP_SC	0xe0000000  /* Normal ; Immediate */
#define MIPS_OP_LLD	0xd0000000  /* Normal ; Immediate */
#define MIPS_OP_SCD	0xf0000000  /* Normal ; Immediate */

// Table A-6: Coprocessor Load/Store Instructions
// See Table B-5: FPU Loads and Stores Using Register + Offset Address Mode
//   LWC1, SWC1, LDC1, SDC1 are floating point instructions (fully
//   specified by their opcode)
//#define MIPS_OP_LWCz
#define MIPS_OP_LWC1	0xc4000000  /* Normal ; Immediate */ 
#define MIPS_OP_LWC2	0xc8000000  /* Normal ; Immediate */ 
//#define MIPS_OP_LWC3	0xcc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define MIPS_OP_SWCz
#define MIPS_OP_SWC1	0xe4000000  /* Normal ; Immediate */
#define MIPS_OP_SWC2	0xe8000000  /* Normal ; Immediate */
//#define MIPS_OP_SWC3	0xec000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define MIPS_OP_LDCz
#define MIPS_OP_LDC1	0xd4000000  /* Normal ; Immediate */	
#define MIPS_OP_LDC2	0xd8000000  /* Normal ; Immediate */
//#define MIPS_OP_LDC3	0xdc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define MIPS_OP_SDCz
#define MIPS_OP_SDC1	0xf4000000  /* Normal ; Immediate */ 
#define MIPS_OP_SDC2	0xf8000000  /* Normal ; Immediate */
//#define MIPS_OP_SDC3	0xfc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3

// Table A-7: FPU Load/Store Instructions Using Register + Register Addressing
// See Table B-6 FPU Loads and Using Register + Register Address Mode
#define MIPS_OP_LWXC1	0x4c000000  /* COP1X  ; Register */
#define MIPS_OP_SWXC1	0x4c000008  /* COP1X  ; Register */
#define MIPS_OP_LDXC1	0x4c000001  /* COP1X  ; Register */
#define MIPS_OP_SDXC1	0x4c000009  /* COP1X  ; Register */


// ---------------------------------------------------------
// ALU
// ---------------------------------------------------------

// Table A-8: ALU Instructions With an Immediate Operand
#define MIPS_OP_ADDI	0x20000000  /* Normal ; Immediate */
#define MIPS_OP_ADDIU	0x24000000  /* Normal ; Immediate */
#define MIPS_OP_SLTI	0x28000000  /* Normal ; Immediate */
#define MIPS_OP_SLTIU	0x2c000000  /* Normal ; Immediate */
#define MIPS_OP_ANDI	0x30000000  /* Normal ; Immediate */
#define MIPS_OP_ORI	0x34000000  /* Normal ; Immediate */
#define MIPS_OP_XORI	0x38000000  /* Normal ; Immediate */
#define MIPS_OP_LUI	0x3c000000  /* Normal ; Immediate */
#define MIPS_OP_DADDI	0x60000000  /* Normal ; Immediate */
#define MIPS_OP_DADDIU	0x64000000  /* Normal ; Immediate */

// Table A-9: 3-Operand ALU Instructions
#define MIPS_OP_ADD	0x00000020  /* Special; Register */
#define MIPS_OP_ADDU	0x00000021  /* Special; Register */
#define MIPS_OP_SUB	0x00000022  /* Special; Register */
#define MIPS_OP_SUBU	0x00000023  /* Special; Register */
#define MIPS_OP_DADD	0x0000002c  /* Special; Register */
#define MIPS_OP_DADDU	0x0000002d  /* Special; Register */
#define MIPS_OP_DSUB	0x0000002e  /* Special; Register */
#define MIPS_OP_DSUBU	0x0000002f  /* Special; Register */
#define MIPS_OP_SLT	0x0000002a  /* Special; Register */
#define MIPS_OP_SLTU	0x0000002b  /* Special; Register */
#define MIPS_OP_AND	0x00000024  /* Special; Register */
#define MIPS_OP_OR	0x00000025  /* Special; Register */
#define MIPS_OP_XOR	0x00000026  /* Special; Register */
#define MIPS_OP_NOR	0x00000027  /* Special; Register */

// Table A-10: Shift Instructions
#define MIPS_OP_SLL	0x00000000  /* Special; Register */
#define MIPS_OP_SRL	0x00000002  /* Special; Register */
#define MIPS_OP_SRA	0x00000003  /* Special; Register */
#define MIPS_OP_SLLV	0x00000004  /* Special; Register */
#define MIPS_OP_SRLV	0x00000006  /* Special; Register */
#define MIPS_OP_SRAV	0x00000007  /* Special; Register */
#define MIPS_OP_DSLL	0x00000038  /* Special; Register */
#define MIPS_OP_DSRL	0x0000003a  /* Special; Register */
#define MIPS_OP_DSRA	0x0000003b  /* Special; Register */
#define MIPS_OP_DSLL32	0x0000003c  /* Special; Register */
#define MIPS_OP_DSRL32	0x0000003e  /* Special; Register */
#define MIPS_OP_DSRA32	0x0000003f  /* Special; Register */
#define MIPS_OP_DSLLV	0x00000014  /* Special; Register */
#define MIPS_OP_DSRLV	0x00000016  /* Special; Register */
#define MIPS_OP_DSRAV	0x00000017  /* Special; Register */

// Table A-11: Multiply/Divide Instructions
#define MIPS_OP_MULT	0x00000018  /* Special; Register */
#define MIPS_OP_MULTU	0x00000019  /* Special; Register */
#define MIPS_OP_DIV	0x0000001a  /* Special; Register */
#define MIPS_OP_DIVU	0x0000001b  /* Special; Register */
#define MIPS_OP_DMULT	0x0000001c  /* Special; Register */
#define MIPS_OP_DMULTU	0x0000001d  /* Special; Register */
#define MIPS_OP_DDIV	0x0000001e  /* Special; Register */
#define MIPS_OP_DDIVU	0x0000001f  /* Special; Register */
#define MIPS_OP_MFHI	0x00000010  /* Special; Register */
#define MIPS_OP_MTHI	0x00000011  /* Special; Register */
#define MIPS_OP_MFLO	0x00000012  /* Special; Register */
#define MIPS_OP_MTLO	0x00000013  /* Special; Register */


// ---------------------------------------------------------
// Jump and Branch
// ---------------------------------------------------------

// Table A-12: Jump Instructions Jumping Within a 256 Megabyte Region
#define MIPS_OP_J	0x08000000  /* Normal ; Jump */
#define MIPS_OP_JAL	0x0c000000  /* Normal ; Jump */

// Table A-13: Jump Instructions to Absolute Address
#define MIPS_OP_JR	0x00000008  /* Special; Register */
#define MIPS_OP_JALR	0x00000009  /* Special; Register */

// Table A-14: PC-Relative Conditional Branch Instr. Comparing 2 Registers
#define MIPS_OP_BEQ	0x10000000  /* Normal ; Immediate */
#define MIPS_OP_BNE	0x14000000  /* Normal ; Immediate */
#define MIPS_OP_BLEZ	0x18000000  /* Normal ; Immediate */
#define MIPS_OP_BGTZ	0x1c000000  /* Normal ; Immediate */
#define MIPS_OP_BEQL	0x50000000  /* Normal ; Immediate */
#define MIPS_OP_BNEL	0x54000000  /* Normal ; Immediate */
#define MIPS_OP_BLEZL	0x58000000  /* Normal ; Immediate */
#define MIPS_OP_BGTZL	0x5c000000  /* Normal ; Immediate */

// Table A-15: PC-Relative Conditional Branch Instr. Comparing Against Zero
#define MIPS_OP_BLTZ	0x04000000  /* RegImm ; Immediate */
#define MIPS_OP_BGEZ	0x04010000  /* RegImm ; Immediate */
#define MIPS_OP_BLTZAL	0x04100000  /* RegImm ; Immediate */
#define MIPS_OP_BGEZAL	0x04110000  /* RegImm ; Immediate */
#define MIPS_OP_BLTZL	0x04020000  /* RegImm ; Immediate */
#define MIPS_OP_BGEZL	0x04030000  /* RegImm ; Immediate */
#define MIPS_OP_BLTZALL	0x04120000  /* RegImm ; Immediate */
#define MIPS_OP_BGEZALL	0x04130000  /* RegImm ; Immediate */


// ---------------------------------------------------------
// Miscellaneous
// ---------------------------------------------------------

// Table A-16: System Call and Breakpoint Instructions
#define MIPS_OP_SYSCALL	0x0000000c  /* Special; Register */
#define MIPS_OP_BREAK	0x0000000d  /* Special; Register */

// Table A-17: Trap-on-Condition Instructions Comparing Two Registers
#define MIPS_OP_TGE	0x00000030  /* Special; Register */
#define MIPS_OP_TGEU	0x00000031  /* Special; Register */
#define MIPS_OP_TLT	0x00000032  /* Special; Register */
#define MIPS_OP_TLTU	0x00000033  /* Special; Register */
#define MIPS_OP_TEQ	0x00000034  /* Special; Register */
#define MIPS_OP_TNE	0x00000036  /* Special; Register */

// Table A-18: Trap-on-Condition Instructions Comparing an Immediate
#define MIPS_OP_TGEI	0x04080000  /* RegImm ; Immediate */
#define MIPS_OP_TGEIU	0x04090000  /* RegImm ; Immediate */
#define MIPS_OP_TLTI	0x040a0000  /* RegImm ; Immediate */
#define MIPS_OP_TLTIU	0x040b0000  /* RegImm ; Immediate */
#define MIPS_OP_TEQI	0x040c0000  /* RegImm ; Immediate */
#define MIPS_OP_TNEI	0x040e0000  /* RegImm ; Immediate */

// Table A-19: Serialization Instructions
#define MIPS_OP_SYNC	0x0000000f  /* Special; Register */

// Table A-20: CPU Conditional Move Instructions
#define MIPS_OP_MOVN	0x0000000b  /* Special; Register */
#define MIPS_OP_MOVZ	0x0000000a  /* Special; Register */

// Table A-21: Prefetch Using Register + Offset Address Mode
#define MIPS_OP_PREF	0xcc000000  /* Normal ; Immediate */

// Table A-22: Prefetch Using Register + Register Address Mode
#define MIPS_OP_PREFX	0x4c00000f  /* COP1X  ; Register */


// ---------------------------------------------------------
// Coprocessor
// ---------------------------------------------------------

// Instead of unnecessarily enumerating all the coprocessor
// instructions (including FPU instructions such as COP1 and COP1X),
// they will generally be subsumed under the appropriate generic
// class-instruction.  FPU load/stores have already been treated as
// individual instructions.

// Table A-24: Coprocessor Operation Instructions
#define MIPS_OP_COP0	0x40000000  /* Normal ; Jump (priveledged, Cf. A-8.3.1) */ 

#define MIPS_OP_COP1	0x44000000  /* Normal ; Jump (FP instructions, Cf. A-8.3.2) */
#define MIPS_OP_COP1X	0x4c000000  /* Normal ; Jump (FP instructions, Cf. A-8.3.2) */

#define MIPS_OP_COP2	0x48000000  /* Normal ; Jump (not-standard, Cf. A-8.3.3) */
//#define MIPS_OP_COP3	0x4c000000  /* Normal ; Jump (depreciated w/ MIPS3) */

// The FPU instructions that can write to CPU registers
#define MIPS_OP_MOVCI	0x00000001  /* Special; Register (MOVF, MOVT) */

#define MIPS_OP_MFC1	0x44000000  /* COP1 */ 
#define MIPS_OP_DMFC1	0x44200000  /* COP1 */
#define MIPS_OP_CFC1	0x44400000  /* COP1 */

// Misc Instruction Aliases
// =========================================
#define MIPS_OP_NOP	0x00000000  /* SLL */
#define MIPS_OP_UNIMP	0x00000000  /* NOP */
#define MIPS_OP_MOVE    OR          /* could also be a ADDU/DADDU */
#define MIPS_OP_LI      MIPS_OP_ADDIU


//***************************************************************************
// Operands
//***************************************************************************

// ---------------------------------------------------------
// register operands
// ---------------------------------------------------------

/* Immediate and Register formats; bits 25-21 */
#define MIPS_OPND_REG_S_MASK  0x03e00000
#define MIPS_OPND_REG_S_SHIFT 21
#define MIPS_OPND_REG_S(insn) \
  (((insn) & MIPS_OPND_REG_S_MASK) >> MIPS_OPND_REG_S_SHIFT)

/* Immediate and Register formats; bits 20-16 */
#define MIPS_OPND_REG_T_MASK  0x001f0000
#define MIPS_OPND_REG_T_SHIFT 16
#define MIPS_OPND_REG_T(insn) \
  (((insn) & MIPS_OPND_REG_T_MASK) >> MIPS_OPND_REG_T_SHIFT)

/* Register format; bits 15-11 */
#define MIPS_OPND_REG_D_MASK  0x0000f800
#define MIPS_OPND_REG_D_SHIFT 11
#define MIPS_OPND_REG_D(insn) \
  (((insn) & MIPS_OPND_REG_D_MASK) >> MIPS_OPND_REG_D_SHIFT)

/* Register format; bits 10-6 */
#define MIPS_OPND_REG_SA_MASK  0x000007c0
#define MIPS_OPND_REG_SA_SHIFT 6
#define MIPS_OPND_REG_SA(insn) \
  (((insn) & MIPS_OPND_REG_SA_MASK) >> MIPS_OPND_REG_SA_SHIFT)


// ---------------------------------------------------------
// immediate operands
// ---------------------------------------------------------

// immediate: 16-bit signed immediate used for: logical operands, arithmetic
// signed operands, load/store address byte offsets, PC-relative
// branch signed instruction displacement
#define MIPS_OPND_IMM_MASK  0x0000ffff
#define MIPS_OPND_IMM_SIGN  0x00008000
#define MIPS_OPND_IMM_SHIFT 0
#define MIPS_OPND_IMM(insn) \
  (((insn) & MIPS_OPND_IMM_MASK) >> MIPS_OPND_IMM_SHIFT)

// insn_index: 26-bit index shifted left two bits to supply the
// low-order 28 bits of the jump target address (PC-region).  The
// remaining upper bits are the corresponding bits of
// the address of the instruction in the delay slot (not the branch
// itself).
#define MIPS_OPND_INSN_INDEX_MASK  0x03ffffff
#define MIPS_OPND_INSN_INDEX_SHIFT 0
#define MIPS_OPND_INSN_INDEX(insn) \
  (((insn) & MIPS_OPND_INSN_INDEX_MASK) >> MIPS_OPND_INSN_INDEX_SHIFT)

#define MIPS_OPND_INSN_INDEX_UPPER_MASK 0xf0000000 /* upper 4 bits */


//***************************************************************************
// Registers
//***************************************************************************

// Register names: see MIPSpro Assembly Language Programmer's Guide,
// Copyright (c) 1996, 1999 Silicon Graphics, Inc.

#define MIPS_REG_0	0  /* $0: Always has the value 0 */
#define MIPS_REG_AT 1  /* $at: Reserved for the assembler */

#define MIPS_REG_V0 2  /* $v0-v1: Used for expr. eval. and to hold int type */
#define MIPS_REG_V1 3  /*   results.  Also, to pass the static link when    */
                       /*   calling nested procedures.                      */

#define MIPS_REG_A0 4  /* $a0-a3: Temporary registers; values not preserved */
#define MIPS_REG_A1 5  /*   across procedure calls.  See manual for slight  */
#define MIPS_REG_A2 6  /*   semantic differences between 32/64 bit CPUs.    */
#define MIPS_REG_A3 7

#define MIPS_REG_T0 8  /* $t0-t7: Temporary registers; values not preserved */
#define MIPS_REG_T1 9  /*   across procedure calls.  See manual for slight  */
#define MIPS_REG_T2 10 /*   semantic differences between 32/64 bit CPUs.    */
#define MIPS_REG_T3 11
#define MIPS_REG_T4 12
#define MIPS_REG_T5 13
#define MIPS_REG_T6 14
#define MIPS_REG_T7 15

#define MIPS_REG_S0 16  /* $s0-s7: Saved registers. Their values must be    */
#define MIPS_REG_S1 17  /*   preserved across procedure calls.              */
#define MIPS_REG_S2 18
#define MIPS_REG_S3 19
#define MIPS_REG_S4 20
#define MIPS_REG_S5 21
#define MIPS_REG_S6 22
#define MIPS_REG_S7 23

#define MIPS_REG_T8 24  /* $t8-t9: Temporary registers. See manual for      */
#define MIPS_REG_T9 25  /*   32 bit CPU                                     */

#define MIPS_REG_K0 26  /* $k0-k1: Reserved for the operating system kernel */
#define MIPS_REG_K1 27

#define MIPS_REG_GP 28  /* $gp: Global pointer. */
#define MIPS_REG_SP 29  /* $sp: Stack pointer. */
#define MIPS_REG_FP 30  /* $fp or $s8: Frame pointer (if needed);           */
#define MIPS_REG_S8 30  /*   otherwise a saved register (cf. as s0-s7)      */
#define MIPS_REG_RA 31  /* $ra Return address; is used for expr. eval.      */

// Automatically accessed by relevant instructions
//#define MIPS_REG_PC  32  /* Program Counter */
//#define MIPS_REG_HI  33  /* HI Mult/Div special reg. */
//#define MIPS_REG_LO  34  /* LO Mult/Div special reg. */

//***************************************************************************

#endif /* isa_instructionset_mips_h */
