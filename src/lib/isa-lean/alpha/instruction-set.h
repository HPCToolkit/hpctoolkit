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
//    alpha.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ALPHA_INSTRUCTIONS_H
#define ALPHA_INSTRUCTIONS_H

#include "inttypes.h"

// Information about Alpha instructions, encoding, conventions, and
// section numbers is from 
//   Alpha Architecture Reference Manual, Fourth Edition 
//   (January 2002; (c) Compaq Computer Corporation 2002)
// It is available at the following anonymous FTP site:
//   ftp.compaq.com/pub/products/alphaCPUdocs/alpha_arch_ref.pdf

// Some of the opcode definitions have been cross-checked with the
// alpha opcode table from GNU's binutils ("opcodes/alpha-opc.c") and
// the file '/usr/include/machine/inst.h'.

// ==========================================================================

// Opcode Classes and Masks

// Instruction Formats: See 3.3 Instruction Formats

// There are five basic Alpha instruction formats.  All instruction
// formats are 32 bits long with a 6-bit major opcode field in bits
// <31:26> of the instruction.

// There are five basic Alpha instruction formats.  All instruction formats
// are 32 bits long with a 6-bit major opcode field in bits <31:26> of the
// instruction. (See 3.3.1 -- 3.3.5, respectively.)
//               31 ......................................................... 0
//   Memory:     opcode(6), Ra(5), Rb(5), Memory_displacemnt(16)
//     (note 1)
//   Branch:     opcode(6), Ra(5), Branch_displacment(21)
//     (note 2)
//   Operate:    opcode(6), Ra(5), Rb(5), SBz(3), LitBit(1), Function(7), Rc(5)
//               opcode(6), Ra(5), Literal(8),    LitBit(1), Function(7), Rc(5)
//   FP Operate: opcode(6), Fa(5), Fb(5), Function(11),                   Fc(5)
//   PALcode:    opcode(6), PALcode function(26)
//
// Note 1: The displacement field is a byte offset. It is
// sign-extended and added to the contents of register Rb to form a
// virtual address. Overflow is ignored in this calculation.
//
// Note 2: The displacement is treated as a longword offset. This
// means it is shifted left two bits (to address a longword boundary),
// sign-extended to 64 bits, and added to the updated PC to form the
// target virtual address. Overflow is ignored in this
// calculation. The target virtual address (va) is computed as
// follows: va <-- PC + {4*SEXT(Branch_disp)}

// ==========================================================================

// Macros for forming the opcodes and Masks for decoding instructions.
// The mask places a 1-bit at every location used to decode an
// instruction.  AND the mask with the instruction to find the opcode.

// See Appendix C (of the "Alpha Architecture Handbook" referenced
// above) and esp. section C.1 and tables C-1, C-2 for the template
// behind this encoding scheme.

// The main opcode 
#define OP(x)		(uint32_t)(((x) & 0x3F) << 26)
#define OP_MASK		0xFC000000    /* opcode: bits 31-26 */

// =========================================

// Memory format instructions (3.3.1) 
#define MEM(oo)		OP(oo)
#define MEM_MASK	OP_MASK

// Memory/Func Code format instructions (3.3.1.1) 
#define MFC(oo,ffff)	(OP(oo) | ((ffff) & 0xFFFF))
#define MFC_MASK	(OP_MASK | 0xFFFF) /* opcode + function bits 15-0 */

// Memory/Branch format instructions (3.3.1.2 and 4.3.3) 
#define MBR(oo,h)	(OP(oo) | (((h) & 0x3) << 14))
#define MBR_MASK	(OP_MASK | 0xC000) /* opcode + hint bits 15-14 */

// =========================================

// Branch format instructions (3.3.2) 
#define BRA(oo)		OP(oo)
#define BRA_MASK	OP_MASK

// =========================================

// Operate format instructions.  The OPRL variant specifies a
// literal second argument. 
#define OPR(oo,ff)	(OP(oo) | (((ff) & 0x7F) << 5))
#define OPRL(oo,ff)	(OPR((oo),(ff)) | 0x1000) 
#define OPR_MASK	(OP_MASK | 0x1FE0) /* opcode + litbit 12 +
					      function bits 11-5 */

// =========================================

// Floating point format instructions 
#define FP(oo,fff)	(OP(oo) | (((fff) & 0x7FF) << 5))
#define FP_MASK		(OP_MASK | 0xFFE0) /* opcode + function bits 15-5 */

// =========================================

// Generic PALcode format instructions 
#define PCD(oo)		OP(oo)
#define PCD_MASK	OP_MASK

// Specific PALcode instructions 
#define SPCD(oo,ffff)	(OP(oo) | ((ffff) & 0x3FFFFFF))
#define SPCD_MASK	0xFFFFFFFF         /* opcode + function bits 25-0 */

// Hardware memory (hw_{ld,st}) instructions (D.1) (use PAL macros) 

// =========================================

// Some useful opcode values for instruction classes

// Use the `Operate format' mask for the following
#define OpIntArith OP(0x10) /* Integer arithmetic instruction opcodes */
#define OpIntLogic OP(0x11) /* Integer logical instruction opcodes */
#define OpIntShift OP(0x12) /* Integer shift instruction opcodes */
#define OpIntMult  OP(0x13) /* Integer multiply instruction opcodes */

// Use the `FP operate' mask for the following
#define OpIntToFlt OP(0x14) /* Integer to FP register move opcodes */
#define OpFltVAX   OP(0x15) /* VAX floating-point instruction opcodes */
#define OpFltIEEE  OP(0x16) /* IEEE floating-point instruction opcodes */
#define OpFltOp    OP(0x17) /* Floating-point Operate instruction opcodes */

// Use the `Memory/Func Code' mask
#define OpMisc     OP(0x18) /* Miscellaneous instruction opcodes */

// Use the `FP operate' mask (but note there are both OPR and FP instructions)
#define OpFltToInt OP(0x1C) /* FP to integer register move opcodes */

// Use the `Memory/Branch format' mask 
#define OpJump     OP(0x1A) /* Jump instruction opcodes */

// ==========================================================================

// Memory Integer/FP Load/Store Instructions (SS 4.2 and 4.8; Tables
// 4-2 and 4-14)

// Memory format instructions

#define LDA		MEM(0x08) 
#define LDAH		MEM(0x09) 
#define LDBU		MEM(0x0A) 
#define UNOP		MEM(0x0B) /*pseudo*/
#define LDQ_U		MEM(0x0B) 
#define LDWU		MEM(0x0C) 
#define STW		MEM(0x0D) 
#define STB		MEM(0x0E) 
#define STQ_U		MEM(0x0F) 

#define LDF		MEM(0x20) 
#define LDG		MEM(0x21) 
#define LDS		MEM(0x22) 
#define LDT		MEM(0x23) 
#define STF		MEM(0x24) 
#define STG		MEM(0x25) 
#define STS		MEM(0x26) 
#define STT		MEM(0x27) 

#define LDL		MEM(0x28) 
#define LDQ		MEM(0x29) 
#define LDL_L		MEM(0x2A) 
#define LDQ_L		MEM(0x2B) 
#define STL		MEM(0x2C) 
#define STQ		MEM(0x2D) 
#define STL_C		MEM(0x2E) 
#define STQ_C		MEM(0x2F) 

// =========================================

// Miscellaneous Instructions (SS 4.11, Table 4-17) 
//   [except AMASK, CALL_PAL, IMPLVER]
// VAX compatibility Instructions (SS 4.12, Table 4-18)

// Memory/Func Code format instructions 

#define TRAPB		MFC(0x18,0x0000) 
#define DRAINT		MFC(0x18,0x0000) /*alias*/
#define EXCB		MFC(0x18,0x0400) 
#define MB		MFC(0x18,0x4000) 
#define WMB		MFC(0x18,0x4400) 
#define FETCH		MFC(0x18,0x8000) 
#define FETCH_M		MFC(0x18,0xA000) 
#define RPCC		MFC(0x18,0xC000) 
#define RC		MFC(0x18,0xE000) 
#define ECB		MFC(0x18,0xE800) 
#define RS		MFC(0x18,0xF000) 
#define WH64		MFC(0x18,0xF800) 

// =========================================

// Integer/FP control Instructions (SS 4.3 and 4.9; Tables 4-3
// and 4-15)

// Memory/Branch format instructions 

#define JMP		MBR(0x1A,0) 
#define JSR		MBR(0x1A,1) 
#define RET		MBR(0x1A,2) 
#define JCR		MBR(0x1A,3) /*alias*/
#define JSR_COROUTINE	MBR(0x1A,3) 

// Branch format instructions 

#define BR		BRA(0x30) 
#define FBEQ		BRA(0x31) 
#define FBLT		BRA(0x32) 
#define FBLE		BRA(0x33) 
#define BSR		BRA(0x34) 
#define FBNE		BRA(0x35) 
#define FBGE		BRA(0x36) 
#define FBGT		BRA(0x37) 
#define BLBC		BRA(0x38) 
#define BEQ		BRA(0x39) 
#define BLT		BRA(0x3A) 
#define BLE		BRA(0x3B) 
#define BLBS		BRA(0x3C) 
#define BNE		BRA(0x3D) 
#define BGE		BRA(0x3E) 
#define BGT		BRA(0x3F) 

// =========================================

// Integer/FP operate Instructions (SS 4.4, 4.5, 4.6 and 4.10; Tables 4-5,
//   4-7, 4-7, 4-16)

// Operate format instructions.  (The literal version of each of these
// instructions can be formed by substituting `OPRL' for `OPR'.  This
// sets the literal bit.)

//   (Mostly) SS 4.4, Table 4-5: Integer Arithmetic

#define SEXTL		OPR(0x10,0x00) /*pseudo*/
#define ADDL		OPR(0x10,0x00) 
#define S4ADDL		OPR(0x10,0x02) 
#define NEGL		OPR(0x10,0x09) /*pseudo*/
#define SUBL		OPR(0x10,0x09) 
#define S4SUBL		OPR(0x10,0x0B) 
#define CMPBGE		OPR(0x10,0x0F) 
#define S8ADDL		OPR(0x10,0x12) 
#define S8SUBL		OPR(0x10,0x1B) 
#define CMPULT		OPR(0x10,0x1D) 
#define ADDQ		OPR(0x10,0x20) 
#define S4ADDQ		OPR(0x10,0x22) 
#define NEGQ		OPR(0x10,0x29) /*pseudo*/
#define SUBQ		OPR(0x10,0x29) 
#define S4SUBQ		OPR(0x10,0x2B) 
#define CMPEQ		OPR(0x10,0x2D) 
#define S8ADDQ		OPR(0x10,0x32) 
#define S8SUBQ		OPR(0x10,0x3B) 
#define CMPULE		OPR(0x10,0x3D) 
#define ADDL_V		OPR(0x10,0x40) 
#define NEGL_V		OPR(0x10,0x49) /*pseudo*/
#define SUBL_V		OPR(0x10,0x49) 
#define CMPLT		OPR(0x10,0x4D) 
#define ADDQ_V		OPR(0x10,0x60) 
#define NEGQ_V		OPR(0x10,0x69) /*pseudo*/
#define SUBQ_V		OPR(0x10,0x69) 
#define CMPLE		OPR(0x10,0x6D) 

//   (Mostly) SS 4.5, Table 4-6: Logical and Shift Instructions
#undef AND 
#define AND		OPR(0x11,0x00) 
#define ANDNOT		OPR(0x11,0x08) /*alias*/
#define BIC		OPR(0x11,0x08) 
#define CMOVLBS		OPR(0x11,0x14) 
#define CMOVLBC		OPR(0x11,0x16) 
#define NOP		OPR(0x11,0x20) /* int nop (see unop) */ /*pseudo*/
#define CLR		OPR(0x11,0x20) /*pseudo*/
#define MOV		OPR(0x11,0x20) /*pseudo*/
#define OR		OPR(0x11,0x20) /*alias*/
#define BIS		OPR(0x11,0x20) 
#define CMOVEQ		OPR(0x11,0x24) 
#define CMOVNE		OPR(0x11,0x26) 
#undef NOT
#define NOT		OPR(0x11,0x28) /*pseudo*/
#define ORNOT		OPR(0x11,0x28) 
#define XOR		OPR(0x11,0x40) 
#define CMOVLT		OPR(0x11,0x44) 
#define CMOVGE		OPR(0x11,0x46) 
#define EQV		OPR(0x11,0x48) 
#define XORNOT		OPR(0x11,0x48) /*alias*/
#define AMASK		OPR(0x11,0x61) 
#define CMOVLE		OPR(0x11,0x64) 
#define CMOVGT		OPR(0x11,0x66) 
#define IMPLVER		OPRL(0x11,0x6C)|(31<<21)|(1<<13)

//   (Mostly) SS 4.6, Table 4-7: Byte Manipulation Instructions
#define MSKBL		OPR(0x12,0x02) 
#define EXTBL		OPR(0x12,0x06) 
#define INSBL		OPR(0x12,0x0B) 
#define MSKWL		OPR(0x12,0x12) 
#define EXTWL		OPR(0x12,0x16) 
#define INSWL		OPR(0x12,0x1B) 
#define MSKLL		OPR(0x12,0x22) 
#define EXTLL		OPR(0x12,0x26) 
#define INSLL		OPR(0x12,0x2B) 
#define ZAP		OPR(0x12,0x30) 
#define ZAPNOT		OPR(0x12,0x31) 
#define MSKQL		OPR(0x12,0x32) 
#define SRL		OPR(0x12,0x34) 
#define EXTQL		OPR(0x12,0x36) 
#define SLL		OPR(0x12,0x39) 
#define INSQL		OPR(0x12,0x3B) 
#define SRA		OPR(0x12,0x3C) 
#define MSKWH		OPR(0x12,0x52) 
#define INSWH		OPR(0x12,0x57) 
#define EXTWH		OPR(0x12,0x5A) 
#define MSKLH		OPR(0x12,0x62) 
#define INSLH		OPR(0x12,0x67) 
#define EXTLH		OPR(0x12,0x6A) 
#define MSKQH		OPR(0x12,0x72) 
#define INSQH		OPR(0x12,0x77) 
#define EXTQH		OPR(0x12,0x7A) 

#define MULL		OPR(0x13,0x00) 
#define MULQ		OPR(0x13,0x20) 
#define UMULH		OPR(0x13,0x30) 
#define MULL_V		OPR(0x13,0x40) 
#define MULQ_V		OPR(0x13,0x60) 

#define SEXTB		OPR(0x1C, 0x00) 
#define SEXTW		OPR(0x1C, 0x01) 
#define CTPOP		OPR(0x1C, 0x30) 
#define CTLZ		OPR(0x1C, 0x32) 
#define CTTZ		OPR(0x1C, 0x33) 

// =========================================

//   SS 4.10, Table 4-16: Floating-Point Operate

// Floating point format instructions 

#define ITOFS		FP(0x14,0x004) 
#define SQRTF_C		FP(0x14,0x00A) 
#define SQRTS_C		FP(0x14,0x00B) 
#define ITOFF		FP(0x14,0x014) 
#define ITOFT		FP(0x14,0x024) 
#define SQRTG_C		FP(0x14,0x02A) 
#define SQRTT_C		FP(0x14,0x02B) 
#define SQRTS_M		FP(0x14,0x04B) 
#define SQRTT_M		FP(0x14,0x06B) 
#define SQRTF		FP(0x14,0x08A) 
#define SQRTS		FP(0x14,0x08B) 
#define SQRTG		FP(0x14,0x0AA) 
#define SQRTT		FP(0x14,0x0AB) 
#define SQRTS_D		FP(0x14,0x0CB) 
#define SQRTT_D		FP(0x14,0x0EB) 
#define SQRTF_UC	FP(0x14,0x10A) 
#define SQRTS_UC	FP(0x14,0x10B) 
#define SQRTG_UC	FP(0x14,0x12A) 
#define SQRTT_UC	FP(0x14,0x12B) 
#define SQRTS_UM	FP(0x14,0x14B) 
#define SQRTT_UM	FP(0x14,0x16B) 
#define SQRTF_U		FP(0x14,0x18A) 
#define SQRTS_U		FP(0x14,0x18B) 
#define SQRTG_U		FP(0x14,0x1AA) 
#define SQRTT_U		FP(0x14,0x1AB) 
#define SQRTS_UD	FP(0x14,0x1CB) 
#define SQRTT_UD	FP(0x14,0x1EB) 
#define SQRTF_SC	FP(0x14,0x40A) 
#define SQRTG_SC	FP(0x14,0x42A) 
#define SQRTF_S		FP(0x14,0x48A) 
#define SQRTG_S		FP(0x14,0x4AA) 
#define SQRTF_SUC	FP(0x14,0x50A) 
#define SQRTS_SUC	FP(0x14,0x50B) 
#define SQRTG_SUC	FP(0x14,0x52A) 
#define SQRTT_SUC	FP(0x14,0x52B) 
#define SQRTS_SUM	FP(0x14,0x54B) 
#define SQRTT_SUM	FP(0x14,0x56B) 
#define SQRTF_SU	FP(0x14,0x58A) 
#define SQRTS_SU	FP(0x14,0x58B) 
#define SQRTG_SU	FP(0x14,0x5AA) 
#define SQRTT_SU	FP(0x14,0x5AB) 
#define SQRTS_SUD	FP(0x14,0x5CB) 
#define SQRTT_SUD	FP(0x14,0x5EB) 
#define SQRTS_SUIC	FP(0x14,0x70B) 
#define SQRTT_SUIC	FP(0x14,0x72B) 
#define SQRTS_SUIM	FP(0x14,0x74B) 
#define SQRTT_SUIM	FP(0x14,0x76B) 
#define SQRTS_SUI	FP(0x14,0x78B) 
#define SQRTT_SUI	FP(0x14,0x7AB) 
#define SQRTS_SUID	FP(0x14,0x7CB) 
#define SQRTT_SUID	FP(0x14,0x7EB) 

#define ADDF_C		FP(0x15,0x000) 
#define SUBF_C		FP(0x15,0x001) 
#define MULF_C		FP(0x15,0x002) 
#define DIVF_C		FP(0x15,0x003) 
#define CVTDG_C		FP(0x15,0x01E) 
#define ADDG_C		FP(0x15,0x020) 
#define SUBG_C		FP(0x15,0x021) 
#define MULG_C		FP(0x15,0x022) 
#define DIVG_C		FP(0x15,0x023) 
#define CVTGF_C		FP(0x15,0x02C) 
#define CVTGD_C		FP(0x15,0x02D) 
#define CVTGQ_C		FP(0x15,0x02F) 
#define CVTQF_C		FP(0x15,0x03C) 
#define CVTQG_C		FP(0x15,0x03E) 
#define ADDF		FP(0x15,0x080) 
#define NEGF		FP(0x15,0x081) /*pseudo*/
#define SUBF		FP(0x15,0x081) 
#define MULF		FP(0x15,0x082) 
#define DIVF		FP(0x15,0x083) 
#define CVTDG		FP(0x15,0x09E) 
#define ADDG		FP(0x15,0x0A0) 
#define NEGG		FP(0x15,0x0A1) /*pseudo*/
#define SUBG		FP(0x15,0x0A1) 
#define MULG		FP(0x15,0x0A2) 
#define DIVG		FP(0x15,0x0A3) 
#define CMPGEQ		FP(0x15,0x0A5) 
#define CMPGLT		FP(0x15,0x0A6) 
#define CMPGLE		FP(0x15,0x0A7) 
#define CVTGF		FP(0x15,0x0AC) 
#define CVTGD		FP(0x15,0x0AD) 
#define CVTGQ		FP(0x15,0x0AF) 
#define CVTQF		FP(0x15,0x0BC) 
#define CVTQG		FP(0x15,0x0BE) 
#define ADDF_UC		FP(0x15,0x100) 
#define SUBF_UC		FP(0x15,0x101) 
#define MULF_UC		FP(0x15,0x102) 
#define DIVF_UC		FP(0x15,0x103) 
#define CVTDG_UC	FP(0x15,0x11E) 
#define ADDG_UC		FP(0x15,0x120) 
#define SUBG_UC		FP(0x15,0x121) 
#define MULG_UC		FP(0x15,0x122) 
#define DIVG_UC		FP(0x15,0x123) 
#define CVTGF_UC	FP(0x15,0x12C) 
#define CVTGD_UC	FP(0x15,0x12D) 
#define CVTGQ_VC	FP(0x15,0x12F) 
#define ADDF_U		FP(0x15,0x180) 
#define SUBF_U		FP(0x15,0x181) 
#define MULF_U		FP(0x15,0x182) 
#define DIVF_U		FP(0x15,0x183) 
#define CVTDG_U		FP(0x15,0x19E) 
#define ADDG_U		FP(0x15,0x1A0) 
#define SUBG_U		FP(0x15,0x1A1) 
#define MULG_U		FP(0x15,0x1A2) 
#define DIVG_U		FP(0x15,0x1A3) 
#define CVTGF_U		FP(0x15,0x1AC) 
#define CVTGD_U		FP(0x15,0x1AD) 
#define CVTGQ_V		FP(0x15,0x1AF) 
#define ADDF_SC		FP(0x15,0x400) 
#define SUBF_SC		FP(0x15,0x401) 
#define MULF_SC		FP(0x15,0x402) 
#define DIVF_SC		FP(0x15,0x403) 
#define CVTDG_SC	FP(0x15,0x41E) 
#define ADDG_SC		FP(0x15,0x420) 
#define SUBG_SC		FP(0x15,0x421) 
#define MULG_SC		FP(0x15,0x422) 
#define DIVG_SC		FP(0x15,0x423) 
#define CVTGF_SC	FP(0x15,0x42C) 
#define CVTGD_SC	FP(0x15,0x42D) 
#define CVTGQ_SC	FP(0x15,0x42F) 
#define ADDF_S		FP(0x15,0x480) 
#define NEGF_S		FP(0x15,0x481) /*pseudo*/
#define SUBF_S		FP(0x15,0x481) 
#define MULF_S		FP(0x15,0x482) 
#define DIVF_S		FP(0x15,0x483) 
#define CVTDG_S		FP(0x15,0x49E) 
#define ADDG_S		FP(0x15,0x4A0) 
#define NEGG_S		FP(0x15,0x4A1) /*pseudo*/
#define SUBG_S		FP(0x15,0x4A1) 
#define MULG_S		FP(0x15,0x4A2) 
#define DIVG_S		FP(0x15,0x4A3) 
#define CMPGEQ_S	FP(0x15,0x4A5) 
#define CMPGLT_S	FP(0x15,0x4A6) 
#define CMPGLE_S	FP(0x15,0x4A7) 
#define CVTGF_S		FP(0x15,0x4AC) 
#define CVTGD_S		FP(0x15,0x4AD) 
#define CVTGQ_S		FP(0x15,0x4AF) 
#define ADDF_SUC	FP(0x15,0x500) 
#define SUBF_SUC	FP(0x15,0x501) 
#define MULF_SUC	FP(0x15,0x502) 
#define DIVF_SUC	FP(0x15,0x503) 
#define CVTDG_SUC	FP(0x15,0x51E) 
#define ADDG_SUC	FP(0x15,0x520) 
#define SUBG_SUC	FP(0x15,0x521) 
#define MULG_SUC	FP(0x15,0x522) 
#define DIVG_SUC	FP(0x15,0x523) 
#define CVTGF_SUC	FP(0x15,0x52C) 
#define CVTGD_SUC	FP(0x15,0x52D) 
#define CVTGQ_SVC	FP(0x15,0x52F) 
#define ADDF_SU		FP(0x15,0x580) 
#define SUBF_SU		FP(0x15,0x581) 
#define MULF_SU		FP(0x15,0x582) 
#define DIVF_SU		FP(0x15,0x583) 
#define CVTDG_SU	FP(0x15,0x59E) 
#define ADDG_SU		FP(0x15,0x5A0) 
#define SUBG_SU		FP(0x15,0x5A1) 
#define MULG_SU		FP(0x15,0x5A2) 
#define DIVG_SU		FP(0x15,0x5A3) 
#define CVTGF_SU	FP(0x15,0x5AC) 
#define CVTGD_SU	FP(0x15,0x5AD) 
#define CVTGQ_SV	FP(0x15,0x5AF) 

#define ADDS_C		FP(0x16,0x000) 
#define SUBS_C		FP(0x16,0x001) 
#define MULS_C		FP(0x16,0x002) 
#define DIVS_C		FP(0x16,0x003) 
#define ADDT_C		FP(0x16,0x020) 
#define SUBT_C		FP(0x16,0x021) 
#define MULT_C		FP(0x16,0x022) 
#define DIVT_C		FP(0x16,0x023) 
#define CVTTS_C		FP(0x16,0x02C) 
#define CVTTQ_C		FP(0x16,0x02F) 
#define CVTQS_C		FP(0x16,0x03C) 
#define CVTQT_C		FP(0x16,0x03E) 
#define ADDS_M		FP(0x16,0x040) 
#define SUBS_M		FP(0x16,0x041) 
#define MULS_M		FP(0x16,0x042) 
#define DIVS_M		FP(0x16,0x043) 
#define ADDT_M		FP(0x16,0x060) 
#define SUBT_M		FP(0x16,0x061) 
#define MULT_M		FP(0x16,0x062) 
#define DIVT_M		FP(0x16,0x063) 
#define CVTTS_M		FP(0x16,0x06C) 
#define CVTTQ_M		FP(0x16,0x06F) 
#define CVTQS_M		FP(0x16,0x07C) 
#define CVTQT_M		FP(0x16,0x07E) 
#define ADDS		FP(0x16,0x080) 
#define NEGS		FP(0x16,0x081) /*pseudo*/
#define SUBS		FP(0x16,0x081) 
#define MULS		FP(0x16,0x082) 
#define DIVS		FP(0x16,0x083) 
#define ADDT		FP(0x16,0x0A0) 
#define NEGT		FP(0x16,0x0A1) /*pseudo*/
#define SUBT		FP(0x16,0x0A1) 
#define MULT		FP(0x16,0x0A2) 
#define DIVT		FP(0x16,0x0A3) 
#define CMPTUN		FP(0x16,0x0A4) 
#define CMPTEQ		FP(0x16,0x0A5) 
#define CMPTLT		FP(0x16,0x0A6) 
#define CMPTLE		FP(0x16,0x0A7) 
#define CVTTS		FP(0x16,0x0AC) 
#define CVTTQ		FP(0x16,0x0AF) 
#define CVTQS		FP(0x16,0x0BC) 
#define CVTQT		FP(0x16,0x0BE) 
#define ADDS_D		FP(0x16,0x0C0) 
#define SUBS_D		FP(0x16,0x0C1) 
#define MULS_D		FP(0x16,0x0C2) 
#define DIVS_D		FP(0x16,0x0C3) 
#define ADDT_D		FP(0x16,0x0E0) 
#define SUBT_D		FP(0x16,0x0E1) 
#define MULT_D		FP(0x16,0x0E2) 
#define DIVT_D		FP(0x16,0x0E3) 
#define CVTTS_D		FP(0x16,0x0EC) 
#define CVTTQ_D		FP(0x16,0x0EF) 
#define CVTQS_D		FP(0x16,0x0FC) 
#define CVTQT_D		FP(0x16,0x0FE) 
#define ADDS_UC		FP(0x16,0x100) 
#define SUBS_UC		FP(0x16,0x101) 
#define MULS_UC		FP(0x16,0x102) 
#define DIVS_UC		FP(0x16,0x103) 
#define ADDT_UC		FP(0x16,0x120) 
#define SUBT_UC		FP(0x16,0x121) 
#define MULT_UC		FP(0x16,0x122) 
#define DIVT_UC		FP(0x16,0x123) 
#define CVTTS_UC	FP(0x16,0x12C) 
#define CVTTQ_VC	FP(0x16,0x12F) 
#define ADDS_UM		FP(0x16,0x140) 
#define SUBS_UM		FP(0x16,0x141) 
#define MULS_UM		FP(0x16,0x142) 
#define DIVS_UM		FP(0x16,0x143) 
#define ADDT_UM		FP(0x16,0x160) 
#define SUBT_UM		FP(0x16,0x161) 
#define MULT_UM		FP(0x16,0x162) 
#define DIVT_UM		FP(0x16,0x163) 
#define CVTTS_UM	FP(0x16,0x16C) 
#define CVTTQ_VM	FP(0x16,0x16F) 
#define ADDS_U		FP(0x16,0x180) 
#define SUBS_U		FP(0x16,0x181) 
#define MULS_U		FP(0x16,0x182) 
#define DIVS_U		FP(0x16,0x183) 
#define ADDT_U		FP(0x16,0x1A0) 
#define SUBT_U		FP(0x16,0x1A1) 
#define MULT_U		FP(0x16,0x1A2) 
#define DIVT_U		FP(0x16,0x1A3) 
#define CVTTS_U		FP(0x16,0x1AC) 
#define CVTTQ_V		FP(0x16,0x1AF) 
#define ADDS_UD		FP(0x16,0x1C0) 
#define SUBS_UD		FP(0x16,0x1C1) 
#define MULS_UD		FP(0x16,0x1C2) 
#define DIVS_UD		FP(0x16,0x1C3) 
#define ADDT_UD		FP(0x16,0x1E0) 
#define SUBT_UD		FP(0x16,0x1E1) 
#define MULT_UD		FP(0x16,0x1E2) 
#define DIVT_UD		FP(0x16,0x1E3) 
#define CVTTS_UD	FP(0x16,0x1EC) 
#define CVTTQ_VD	FP(0x16,0x1EF) 
#define CVTST		FP(0x16,0x2AC) 
#define ADDS_SUC	FP(0x16,0x500) 
#define SUBS_SUC	FP(0x16,0x501) 
#define MULS_SUC	FP(0x16,0x502) 
#define DIVS_SUC	FP(0x16,0x503) 
#define ADDT_SUC	FP(0x16,0x520) 
#define SUBT_SUC	FP(0x16,0x521) 
#define MULT_SUC	FP(0x16,0x522) 
#define DIVT_SUC	FP(0x16,0x523) 
#define CVTTS_SUC	FP(0x16,0x52C) 
#define CVTTQ_SVC	FP(0x16,0x52F) 
#define ADDS_SUM	FP(0x16,0x540) 
#define SUBS_SUM	FP(0x16,0x541) 
#define MULS_SUM	FP(0x16,0x542) 
#define DIVS_SUM	FP(0x16,0x543) 
#define ADDT_SUM	FP(0x16,0x560) 
#define SUBT_SUM	FP(0x16,0x561) 
#define MULT_SUM	FP(0x16,0x562) 
#define DIVT_SUM	FP(0x16,0x563) 
#define CVTTS_SUM	FP(0x16,0x56C) 
#define CVTTQ_SVM	FP(0x16,0x56F) 
#define ADDS_SU		FP(0x16,0x580) 
#define NEGS_SU		FP(0x16,0x581) /*pseudo*/
#define SUBS_SU		FP(0x16,0x581) 
#define MULS_SU		FP(0x16,0x582) 
#define DIVS_SU		FP(0x16,0x583) 
#define ADDT_SU		FP(0x16,0x5A0) 
#define NEGT_SU		FP(0x16,0x5A1) /*pseudo*/
#define SUBT_SU		FP(0x16,0x5A1) 
#define MULT_SU		FP(0x16,0x5A2) 
#define DIVT_SU		FP(0x16,0x5A3) 
#define CMPTUN_SU	FP(0x16,0x5A4) 
#define CMPTEQ_SU	FP(0x16,0x5A5) 
#define CMPTLT_SU	FP(0x16,0x5A6) 
#define CMPTLE_SU	FP(0x16,0x5A7) 
#define CVTTS_SU	FP(0x16,0x5AC) 
#define CVTTQ_SV	FP(0x16,0x5AF) 
#define ADDS_SUD	FP(0x16,0x5C0) 
#define SUBS_SUD	FP(0x16,0x5C1) 
#define MULS_SUD	FP(0x16,0x5C2) 
#define DIVS_SUD	FP(0x16,0x5C3) 
#define ADDT_SUD	FP(0x16,0x5E0) 
#define SUBT_SUD	FP(0x16,0x5E1) 
#define MULT_SUD	FP(0x16,0x5E2) 
#define DIVT_SUD	FP(0x16,0x5E3) 
#define CVTTS_SUD	FP(0x16,0x5EC) 
#define CVTTQ_SVD	FP(0x16,0x5EF) 
#define CVTST_S		FP(0x16,0x6AC) 
#define ADDS_SUIC	FP(0x16,0x700) 
#define SUBS_SUIC	FP(0x16,0x701) 
#define MULS_SUIC	FP(0x16,0x702) 
#define DIVS_SUIC	FP(0x16,0x703) 
#define ADDT_SUIC	FP(0x16,0x720) 
#define SUBT_SUIC	FP(0x16,0x721) 
#define MULT_SUIC	FP(0x16,0x722) 
#define DIVT_SUIC	FP(0x16,0x723) 
#define CVTTS_SUIC	FP(0x16,0x72C) 
#define CVTTQ_SVIC	FP(0x16,0x72F) 
#define CVTQS_SUIC	FP(0x16,0x73C) 
#define CVTQT_SUIC	FP(0x16,0x73E) 
#define ADDS_SUIM	FP(0x16,0x740) 
#define SUBS_SUIM	FP(0x16,0x741) 
#define MULS_SUIM	FP(0x16,0x742) 
#define DIVS_SUIM	FP(0x16,0x743) 
#define ADDT_SUIM	FP(0x16,0x760) 
#define SUBT_SUIM	FP(0x16,0x761) 
#define MULT_SUIM	FP(0x16,0x762) 
#define DIVT_SUIM	FP(0x16,0x763) 
#define CVTTS_SUIM	FP(0x16,0x76C) 
#define CVTTQ_SVIM	FP(0x16,0x76F) 
#define CVTQS_SUIM	FP(0x16,0x77C) 
#define CVTQT_SUIM	FP(0x16,0x77E) 
#define ADDS_SUI	FP(0x16,0x780) 
#define NEGS_SUI	FP(0x16,0x781) /*pseudo*/
#define SUBS_SUI	FP(0x16,0x781) 
#define MULS_SUI	FP(0x16,0x782) 
#define DIVS_SUI	FP(0x16,0x783) 
#define ADDT_SUI	FP(0x16,0x7A0) 
#define NEGT_SUI	FP(0x16,0x7A1) /*pseudo*/
#define SUBT_SUI	FP(0x16,0x7A1) 
#define MULT_SUI	FP(0x16,0x7A2) 
#define DIVT_SUI	FP(0x16,0x7A3) 
#define CVTTS_SUI	FP(0x16,0x7AC) 
#define CVTTQ_SVI	FP(0x16,0x7AF) 
#define CVTQS_SUI	FP(0x16,0x7BC) 
#define CVTQT_SUI	FP(0x16,0x7BE) 
#define ADDS_SUID	FP(0x16,0x7C0) 
#define SUBS_SUID	FP(0x16,0x7C1) 
#define MULS_SUID	FP(0x16,0x7C2) 
#define DIVS_SUID	FP(0x16,0x7C3) 
#define ADDT_SUID	FP(0x16,0x7E0) 
#define SUBT_SUID	FP(0x16,0x7E1) 
#define MULT_SUID	FP(0x16,0x7E2) 
#define DIVT_SUID	FP(0x16,0x7E3) 
#define CVTTS_SUID	FP(0x16,0x7EC) 
#define CVTTQ_SVID	FP(0x16,0x7EF) 
#define CVTQS_SUID	FP(0x16,0x7FC) 
#define CVTQT_SUID	FP(0x16,0x7FE) 

#define CVTLQ		FP(0x17,0x010) 
#define FNOP		FP(0x17,0x020) /*pseudo*/
#define FCLR		FP(0x17,0x020) /*pseudo*/
#define FABS		FP(0x17,0x020) /*pseudo*/
#define FMOV		FP(0x17,0x020) /*pseudo*/
#define CPYS		FP(0x17,0x020) 
#define FNEG		FP(0x17,0x021) /*pseudo*/
#define CPYSN		FP(0x17,0x021) 
#define CPYSE		FP(0x17,0x022) 
#define MT_FPCR		FP(0x17,0x024) 
#define MF_FPCR		FP(0x17,0x025) 
#define FCMOVEQ		FP(0x17,0x02A) 
#define FCMOVNE		FP(0x17,0x02B) 
#define FCMOVLT		FP(0x17,0x02C) 
#define FCMOVGE		FP(0x17,0x02D) 
#define FCMOVLE		FP(0x17,0x02E) 
#define FCMOVGT		FP(0x17,0x02F) 
#define CVTQL		FP(0x17,0x030) 
#define CVTQL_V		FP(0x17,0x130) 
#define CVTQL_SV	FP(0x17,0x530) 

#define FTOIT		FP(0x1C, 0x70) 
#define FTOIS		FP(0x1C, 0x78) 

// =========================================

// (Mostly) Multimedia Instructions (SS 4.13, Table 4-19(?))

#define PERR		OPR(0x1C, 0x31) 
#define UNPKBW		OPR(0x1C, 0x34) 
#define UNPKBL		OPR(0x1C, 0x35) 
#define PKWB		OPR(0x1C, 0x36) 
#define PKLB		OPR(0x1C, 0x37) 
#define MINSB8		OPR(0x1C, 0x38) 
#define MINSW4		OPR(0x1C, 0x39) 
#define MINUB8		OPR(0x1C, 0x3A) 
#define MINUW4		OPR(0x1C, 0x3B) 
#define MAXUB8		OPR(0x1C, 0x3C) 
#define MAXUW4		OPR(0x1C, 0x3D) 
#define MAXSB8		OPR(0x1C, 0x3E) 
#define MAXSW4		OPR(0x1C, 0x3F) 

// =========================================

// Generic PALcode format instructions 

// Specific PALcode instructions 

#define HALT		SPCD(0x00,0x0000) 
#define DRAINA		SPCD(0x00,0x0002) 
#define BPT		SPCD(0x00,0x0080) 
#define CALLSYS		SPCD(0x00,0x0083) 
#define CHMK		SPCD(0x00,0x0083) 
#define IMB		SPCD(0x00,0x0086) 
#define CALL_PAL	PCD(0x00) 
#define PAL		PCD(0x00) /*alias*/
#define PAL19		PCD(0x19) 
#define PAL1B		PCD(0x1B) 
#define PAL1D		PCD(0x1D) 
#define PAL1E		PCD(0x1E) 
#define PAL1F		PCD(0x1F) 

// Hardware and Hardware memory (hw_{ld,st}) instructions
//   These are essentially instruction classes.  Implementations
//   provide *many* variations of each instruction.

#define HW_MFPR		PCD(0x19) /* Move data from processor register */
#define HW_LD		PCD(0x1B) /* Load data from memory */
#define HW_MTPR		PCD(0x1D) /* Move data to processor register */
#define HW_REI		PCD(0x1E) /* Return from PALmode exception */
#define HW_ST		PCD(0x1F) /* Store data in memory */

// ==========================================================================

// Masks for looking at register operands in Alpha instructions

#define REG_A_MASK  0x03e00000 /* Register format; bits 25-21 */
#define REG_A_SHIFT 21
#define REG_A(INSN) (((INSN) & REG_A_MASK) >> REG_A_SHIFT)

#define REG_B_MASK  0x001f0000 /* Register format; bits 20-16 */
#define REG_B_SHIFT 16
#define REG_B(INSN) (((INSN) & REG_B_MASK) >> REG_B_SHIFT)

#define REG_C_MASK  0x0000001f /* Register format; bits 4-0 */
#define REG_C_SHIFT 0
#define REG_C(INSN) (((INSN) & REG_C_MASK) >> REG_C_SHIFT)

// Masks for looking at immediates in Alpha instructions.

// immediate:
// literal for operate instructions: If bit <12> of the instruction is
// 1, an 8-bit zero-extended literal constant is formed by bits
// <20:13> of the instruction. The literal is interpreted as a
// positive integer between 0 and 255 and is zero-extended to 64 bits.
#define IMM_MASK  0x001fe000 /* bits 20-13 */
#define IMM_SIGN  0x00000000 /* unsigned */
#define IMM_SHIFT 13
#define IMM(INSN) (((INSN) & IMM_MASK) >> IMM_SHIFT)

// mem_disp:
// memory displacement: 16-bit sign-extended displacement for load/stores.
#define MEM_DISP_MASK  0x0000ffff /* bits 15-0 */
#define MEM_DISP_SIGN  0x00008000 /* bit 15 */
#define MEM_DISP_SHIFT 0
#define MEM_DISP(INSN) (((INSN) & MEM_DISP_MASK) >> MEM_DISP_SHIFT)

// br_disp:
// branch displacement: 21-bit displacement is treated as
// a longword offset. This means it is shifted left two bits (to
// address a longword boundary), sign-extended to 64 bits, and added
// to the updated PC to form the target virtual address. Overflow is
// ignored in this calculation. The target virtual address (va) is
// computed as follows: va <-- PC + {4*SEXT(Branch_disp)}
#define BR_DISP_MASK  0x001fffff /* bits 20-0 */
#define BR_DISP_SIGN  0x00100000 /* bit 20 */
#define BR_DISP_SHIFT 0
#define BR_DISP(INSN) (((INSN) & BR_DISP_MASK) >> BR_DISP_SHIFT)

// ==========================================================================

// Register names: see Tru64 UNIX Assembly Language Programmer's
// Guide. Copyright (c) 2000 Compaq Computer Corporation.

// There are 32 integer registers (R0 through R31), each 64 bits wide.
// R31 always contains the value 0. 
//
// There are 32 floating-point registers ($f0 to $f31), each of which
// is 64 bits wide.  Each register can hold one single-precision
// (32-bit) value or one double-precision (64-bit) value.
// Floating-point register $f31 always contains the value 0.0.

#define REG_V0	0  /* $v0-v1: Used for expr. eval. and to hold int type fnct */
                   /*   results.  Not preserved across procedure calls       */
#define REG_T0	1  /* $t0-t7: Temporary registers used for expr. eval. Not   */
#define REG_T1	2  /*   preserved across procedure calls.                    */
#define REG_T2	3  
#define REG_T3	4  
#define REG_T4	5  
#define REG_T5	6  
#define REG_T6	7
#define REG_T7	8  

#define REG_S0	9   /* $s0-s5: Saved registers. Preserved across procedure   */
#define REG_S1	10  /*    calls.                                             */
#define REG_S2	11
#define REG_S3	12
#define REG_S4	13
#define REG_S5	14

#define REG_FP	15  /* $fp or $s6: Contains the frame pointer (if needed);   */
#define REG_S6	15  /*   otherwise a saved register (such as s0-s5)          */

#define REG_A0	16  /* $a0-a5: Used to pass the first six integer type actual*/
#define REG_A1	17  /*   arguments. Not preserved across procedure calls.    */
#define REG_A2	18
#define REG_A3	19
#define REG_A4	20
#define REG_A5	21

#define REG_T8	22  /* $t8-t11: Temporary registers used for expr. eval.     */
#define REG_T9	23  /*   Not preserved across procedure calls.               */
#define REG_T10	24  
#define REG_T11	25  

#define REG_RA	26  /* $ra: Contains the return address. Preserved across    */
                    /*    procedure calls.                                   */
#define REG_PV	27  /* $pv: Contains the procedure value and used for expr.  */
                    /* eval. Not preserved across procedure calls.           */
#define REG_AT	28  /* $at: AT Reserved for the assembler. Not preserved.    */
#define REG_GP	29  /* $gp: Contains the global pointer. Not preserved.      */
#define REG_SP	30  /* $sp: Contains the stack pointer. Preserved.           */
#define REG_31	31  /* $31: always 0 */

// Automatically accessed by relevant instructions
//#define REG_PC  32  /* Program Counter */

// The Program Counter (PC) is a special register that addresses the
// instruction stream. As each instruction is decoded, the PC is
// advanced to the next sequential instruction. This is referred to as
// the updated PC. Any instruction that uses the value of the PC will
// use the updated PC. The PC includes only bits <63:2> with bits
// <1:0> treated as RAZ/IGN. This quantity is a long-word-aligned byte
// address. The PC is an implied operand on conditional branch and
// subroutine jump instructions. The PC is not accessible as an
// integer register.

// ==========================================================================


#endif
