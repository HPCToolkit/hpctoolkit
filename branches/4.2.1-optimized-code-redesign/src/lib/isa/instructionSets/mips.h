// -*-Mode: C;-*-
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
//    mips.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef MIPS_INSTRUCTIONS_H
#define MIPS_INSTRUCTIONS_H


// Information about MIPS instructions, encoding, conventions, and
// section numbers are from "MIPS IV Instruction Set," Revision 3.2,
// By Charles Price, September, 1995 MIPS Technologies, Inc.


// ==========================================================================

// Opcode Classes and Masks

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
#define OP_MASK        0xfc000000     /* opcode: bits 31-26 */
#define OPSpecial_MASK 0xfc00003f     /* opcode + format bits: 5-0 */
#define OPRegImm_MASK  0xfc1f0000     /* opcode + rt bits: 20-16 */
#define OPCop1x_MASK   0xfc00003f     /* opcode + function bits: 5-0 */

//   special purpose mask: cop1 inst's that modify CPU registers
#define OPCop1_mv2cpu_MASK 0xffe007ff /* opcode + bits 25-21 + zeroed 10-0 */

// Opcode values that specify classes rather than a specific instruction
#define OPSpecial      0x00000000     /* opcode for OPSpecial class */
#define OPRegImm       0x04000000     /* opcode for OPRegImm class */
#define OPCop0         0x40000000     /* opcode for OPCop0 class */
#define OPCop1         0x44000000     /* opcode for OPCop1 class */
#define OPCop2         0x48000000     /* opcode for OPCop2 class */
#define OPCop1x        0x4c000000     /* opcode for OPCop1x class */

// ==========================================================================

// Instructions
//   After each instruction follows its class and format, respectively
  
// Load and Store
// =========================================

// Table A-3: Normal CPU Load/Store Instructions
#define LB	0x80000000  /* Normal ; Immediate */
#define LBU	0x90000000  /* Normal ; Immediate */
#define SB	0xa0000000  /* Normal ; Immediate */
#define LH	0x84000000  /* Normal ; Immediate */
#define LHU	0x94000000  /* Normal ; Immediate */
#define SH	0xa4000000  /* Normal ; Immediate */
#define LW	0x8c000000  /* Normal ; Immediate */
#define LWU	0x9c000000  /* Normal ; Immediate */
#define SW	0xac000000  /* Normal ; Immediate */
#define LD	0xdc000000  /* Normal ; Immediate */
#define SD	0xfc000000  /* Normal ; Immediate */

// Table A-4: Unaligned CPU Load/Store Instructions
#define LWL	0x88000000  /* Normal ; Immediate */
#define LWR	0x98000000  /* Normal ; Immediate */
#define SWL	0xa8000000  /* Normal ; Immediate */
#define SWR	0xb8000000  /* Normal ; Immediate */
#define LDL	0x68000000  /* Normal ; Immediate */
#define LDR	0x6c000000  /* Normal ; Immediate */
#define SDL	0xb0000000  /* Normal ; Immediate */
#define SDR	0xb4000000  /* Normal ; Immediate */

// Table A-5: Atomic Update CPU Load/Store Instructions
#define LL	0xc0000000  /* Normal ; Immediate */
#define SC	0xe0000000  /* Normal ; Immediate */
#define LLD	0xd0000000  /* Normal ; Immediate */
#define SCD	0xf0000000  /* Normal ; Immediate */

// Table A-6: Coprocessor Load/Store Instructions
// See Table B-5: FPU Loads and Stores Using Register + Offset Address Mode
//   LWC1, SWC1, LDC1, SDC1 are floating point instructions (fully
//   specified by their opcode)
//#define LWCz
#define LWC1	0xc4000000  /* Normal ; Immediate */ 
#define LWC2	0xc8000000  /* Normal ; Immediate */ 
//#define LWC3	0xcc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define SWCz
#define SWC1	0xe4000000  /* Normal ; Immediate */
#define SWC2	0xe8000000  /* Normal ; Immediate */
//#define SWC3	0xec000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define LDCz
#define LDC1	0xd4000000  /* Normal ; Immediate */	
#define LDC2	0xd8000000  /* Normal ; Immediate */
//#define LDC3	0xdc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3
//#define SDCz
#define SDC1	0xf4000000  /* Normal ; Immediate */ 
#define SDC2	0xf8000000  /* Normal ; Immediate */
//#define SDC3	0xfc000000  /* Normal ; Immediate */ // depreciated w/ MIPS3

// Table A-7: FPU Load/Store Instructions Using Register + Register Addressing
// See Table B-6 FPU Loads and Using Register + Register Address Mode
#define LWXC1	0x4c000000  /* COP1X  ; Register */
#define SWXC1	0x4c000008  /* COP1X  ; Register */
#define LDXC1	0x4c000001  /* COP1X  ; Register */
#define SDXC1	0x4c000009  /* COP1X  ; Register */

// ALU
// =========================================

// Table A-8: ALU Instructions With an Immediate Operand
#define ADDI	0x20000000  /* Normal ; Immediate */
#define ADDIU	0x24000000  /* Normal ; Immediate */
#define SLTI	0x28000000  /* Normal ; Immediate */
#define SLTIU	0x2c000000  /* Normal ; Immediate */
#define ANDI	0x30000000  /* Normal ; Immediate */
#define ORI	0x34000000  /* Normal ; Immediate */
#define XORI	0x38000000  /* Normal ; Immediate */
#define LUI	0x3c000000  /* Normal ; Immediate */
#define DADDI	0x60000000  /* Normal ; Immediate */
#define DADDIU	0x64000000  /* Normal ; Immediate */

// Table A-9: 3-Operand ALU Instructions
#define ADD	0x00000020  /* Special; Register */
#define ADDU	0x00000021  /* Special; Register */
#define SUB	0x00000022  /* Special; Register */
#define SUBU	0x00000023  /* Special; Register */
#define DADD	0x0000002c  /* Special; Register */
#define DADDU	0x0000002d  /* Special; Register */
#define DSUB	0x0000002e  /* Special; Register */
#define DSUBU	0x0000002f  /* Special; Register */
#define SLT	0x0000002a  /* Special; Register */
#define SLTU	0x0000002b  /* Special; Register */
#undef AND 
#define AND	0x00000024  /* Special; Register */
#define OR	0x00000025  /* Special; Register */
#define XOR	0x00000026  /* Special; Register */
#define NOR	0x00000027  /* Special; Register */

// Table A-10: Shift Instructions
#define SLL	0x00000000  /* Special; Register */
#define SRL	0x00000002  /* Special; Register */
#define SRA	0x00000003  /* Special; Register */
#define SLLV	0x00000004  /* Special; Register */
#define SRLV	0x00000006  /* Special; Register */
#define SRAV	0x00000007  /* Special; Register */
#define DSLL	0x00000038  /* Special; Register */
#define DSRL	0x0000003a  /* Special; Register */
#define DSRA	0x0000003b  /* Special; Register */
#define DSLL32	0x0000003c  /* Special; Register */
#define DSRL32	0x0000003e  /* Special; Register */
#define DSRA32	0x0000003f  /* Special; Register */
#define DSLLV	0x00000014  /* Special; Register */
#define DSRLV	0x00000016  /* Special; Register */
#define DSRAV	0x00000017  /* Special; Register */

// Table A-11: Multiply/Divide Instructions
#define MULT	0x00000018  /* Special; Register */
#define MULTU	0x00000019  /* Special; Register */
#define DIV	0x0000001a  /* Special; Register */
#define DIVU	0x0000001b  /* Special; Register */
#define DMULT	0x0000001c  /* Special; Register */
#define DMULTU	0x0000001d  /* Special; Register */
#define DDIV	0x0000001e  /* Special; Register */
#define DDIVU	0x0000001f  /* Special; Register */
#define MFHI	0x00000010  /* Special; Register */
#define MTHI	0x00000011  /* Special; Register */
#define MFLO	0x00000012  /* Special; Register */
#define MTLO	0x00000013  /* Special; Register */

// Jump and Branch
// =========================================

// Table A-12: Jump Instructions Jumping Within a 256 Megabyte Region
#define J	0x08000000  /* Normal ; Jump */
#define JAL	0x0c000000  /* Normal ; Jump */

// Table A-13: Jump Instructions to Absolute Address
#define JR	0x00000008  /* Special; Register */
#define JALR	0x00000009  /* Special; Register */

// Table A-14: PC-Relative Conditional Branch Instr. Comparing 2 Registers
#define BEQ	0x10000000  /* Normal ; Immediate */
#define BNE	0x14000000  /* Normal ; Immediate */
#define BLEZ	0x18000000  /* Normal ; Immediate */
#define BGTZ	0x1c000000  /* Normal ; Immediate */
#define BEQL	0x50000000  /* Normal ; Immediate */
#define BNEL	0x54000000  /* Normal ; Immediate */
#define BLEZL	0x58000000  /* Normal ; Immediate */
#define BGTZL	0x5c000000  /* Normal ; Immediate */

// Table A-15: PC-Relative Conditional Branch Instr. Comparing Against Zero
#define BLTZ	0x04000000  /* RegImm ; Immediate */
#define BGEZ	0x04010000  /* RegImm ; Immediate */
#define BLTZAL	0x04100000  /* RegImm ; Immediate */
#define BGEZAL	0x04110000  /* RegImm ; Immediate */
#define BLTZL	0x04020000  /* RegImm ; Immediate */
#define BGEZL	0x04030000  /* RegImm ; Immediate */
#define BLTZALL	0x04120000  /* RegImm ; Immediate */
#define BGEZALL	0x04130000  /* RegImm ; Immediate */

// Miscellaneous
// =========================================

// Table A-16: System Call and Breakpoint Instructions
#define SYSCALL	0x0000000c  /* Special; Register */
#define BREAK	0x0000000d  /* Special; Register */

// Table A-17: Trap-on-Condition Instructions Comparing Two Registers
#define TGE	0x00000030  /* Special; Register */
#define TGEU	0x00000031  /* Special; Register */
#define TLT	0x00000032  /* Special; Register */
#define TLTU	0x00000033  /* Special; Register */
#define TEQ	0x00000034  /* Special; Register */
#define TNE	0x00000036  /* Special; Register */

// Table A-18: Trap-on-Condition Instructions Comparing an Immediate
#define TGEI	0x04080000  /* RegImm ; Immediate */
#define TGEIU	0x04090000  /* RegImm ; Immediate */
#define TLTI	0x040a0000  /* RegImm ; Immediate */
#define TLTIU	0x040b0000  /* RegImm ; Immediate */
#define TEQI	0x040c0000  /* RegImm ; Immediate */
#define TNEI	0x040e0000  /* RegImm ; Immediate */

// Table A-19: Serialization Instructions
#define SYNC	0x0000000f  /* Special; Register */

// Table A-20: CPU Conditional Move Instructions
#define MOVN	0x0000000b  /* Special; Register */
#define MOVZ	0x0000000a  /* Special; Register */

// Table A-21: Prefetch Using Register + Offset Address Mode
#define PREF	0xcc000000  /* Normal ; Immediate */

// Table A-22: Prefetch Using Register + Register Address Mode
#define PREFX	0x4c00000f  /* COP1X  ; Register */

// Coprocessor
// =========================================
//   Instead of unnecessarily enumerating all the coprocessor
//   instructions (including FPU instructions such as COP1 and COP1X),
//   they will generally be subsumed under the appropriate generic
//   class-instruction.  FPU load/stores have already been treated as
//   individual instructions.

// Table A-24: Coprocessor Operation Instructions
#define COP0	0x40000000  /* Normal ; Jump (priveledged, Cf. A-8.3.1) */ 

#define COP1	0x44000000  /* Normal ; Jump (FP instructions, Cf. A-8.3.2) */
#define COP1X	0x4c000000  /* Normal ; Jump (FP instructions, Cf. A-8.3.2) */

#define COP2	0x48000000  /* Normal ; Jump (not-standard, Cf. A-8.3.3) */
//#define COP3	0x4c000000  /* Normal ; Jump (depreciated w/ MIPS3) */

// The FPU instructions that can write to CPU registers
#define MOVCI	0x00000001  /* Special; Register (MOVF, MOVT) */

#define MFC1	0x44000000  /* COP1 */ 
#define DMFC1	0x44200000  /* COP1 */
#define CFC1	0x44400000  /* COP1 */

// Misc Instruction Aliases
// =========================================
#define NOP	0x00000000  /* SLL */
#define UNIMP	0x00000000  /* NOP */

// ==========================================================================

// Masks for looking at register operands in MIPS instructions

#define REG_S_MASK  0x03e00000 /* Immediate and Register formats; bits 25-21 */
#define REG_S_SHIFT 21
#define REG_S(INSN) (((INSN) & REG_S_MASK) >> REG_S_SHIFT)

#define REG_T_MASK  0x001f0000 /* Immediate and Register formats; bits 20-16 */
#define REG_T_SHIFT 16
#define REG_T(INSN) (((INSN) & REG_T_MASK) >> REG_T_SHIFT)

#define REG_D_MASK  0x0000f800 /* Register format; bits 15-11 */
#define REG_D_SHIFT 11
#define REG_D(INSN) (((INSN) & REG_D_MASK) >> REG_D_SHIFT)

#define SA_MASK  0x000007c0 /* Register format; bits 10-6 */
#define SA_SHIFT 6
#define SA(INSN) (((INSN) & SA_MASK) >> SA_SHIFT)

// Masks for looking at immediates in MIPS instructions

// immediate: 16-bit signed immediate used for: logical operands, arithmetic
// signed operands, load/store address byte offsets, PC-relative
// branch signed instruction displacement
#define IMM_MASK  0x0000ffff
#define IMM_SIGN  0x00008000
#define IMM_SHIFT 0
#define IMM(INSN) (((INSN) & IMM_MASK) >> IMM_SHIFT)

// insn_index: 26-bit index shifted left two bits to supply the
// low-order 28 bits of the jump target address (PC-region).  The
// remaining upper bits are the corresponding bits of
// the address of the instruction in the delay slot (not the branch
// itself).
#define INSN_INDEX_MASK  0x03ffffff
#define INSN_INDEX_SHIFT 0
#define INSN_INDEX(INSN) (((INSN) & INSN_INDEX_MASK) >> INSN_INDEX_SHIFT)

#define INSN_INDEX_UPPER_MASK 0xf0000000 /* upper 4 bits */

// ==========================================================================

// Register names: see MIPSpro Assembly Language Programmer's Guide,
// Copyright (c) 1996, 1999 Silicon Graphics, Inc.

#define REG_0	0  /* $0: Always has the value 0 */
#define REG_AT	1  /* $at: Reserved for the assembler */

#define REG_V0	2  /* $v0-v1: Used for expr. eval. and to hold int type fnct */
#define REG_V1	3  /*   results.  Also, to pass the static link when calling */
                   /*   nested procedures.                                   */

#define REG_A0	4  /* $a0-a3: Temporary registers; values are not preserved  */
#define REG_A1	5  /*   across procedure calls.  See manual for slight       */
#define REG_A2	6  /*   semantic differences between 32 and 64 bit CPUs.     */
#define REG_A3	7

#define REG_T0	8  /* $t0-t7: Temporary registers; values are not preserved  */
#define REG_T1	9  /*   across procedure calls.  See manual for slight       */
#define REG_T2	10 /*   semantic differences between 32 and 64 bit CPUs.     */
#define REG_T3	11
#define REG_T4	12
#define REG_T5	13
#define REG_T6	14
#define REG_T7	15

#define REG_S0	16  /* $s0-s7: Saved registers. Their values must be         */
#define REG_S1	17  /*   preserved across procedure calls.                   */
#define REG_S2	18
#define REG_S3	19
#define REG_S4	20
#define REG_S5	21
#define REG_S6	22
#define REG_S7	23

#define REG_T8	24  /* $t8-t9: Temporary registers. See manual for 32 bit    */
#define REG_T9	25  /*   CPU                                                 */

#define REG_K0	26  /* $k0-k1: Reserved for the operating system kernel      */
#define REG_K1	27

#define REG_GP	28  /* $gp: Contains the global pointer. */
#define REG_SP	29  /* $sp: Contains the stack pointer. */
#define REG_FP	30  /* $fp or $s8: Contains the frame pointer (if needed);   */
#define REG_S8	30  /*   otherwise a saved register (such as s0-s7)          */
#define REG_RA	31  /* $ra Contains return address; is used for expr. eval.  */

// Automatically accessed by relevant instructions
//#define REG_PC  32  /* Program Counter */
//#define REG_HI  33  /* HI Mult/Div special reg. */
//#define REG_LO  34  /* LO Mult/Div special reg. */

// ==========================================================================

#define MOV OR

#endif
