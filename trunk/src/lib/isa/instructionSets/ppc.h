// -*-Mode: C++;-*- // technically C99
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   PPC64 ISA
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   []
//
//***************************************************************************

#ifndef isa_instructionset_ppc_h
#define isa_instructionset_ppc_h

#include <inttypes.h>

//****************************************************************************
// 
//****************************************************************************

//***************************************************************************
// Registers
//***************************************************************************

#define PPC_REG_R0  0
#define PPC_REG_RA  PPC_REG_R0 /* typical */

#define PPC_REG_R1  1
#define PPC_REG_SP  PPC_REG_R1 /* typical */
#define PPC_REG_FP  PPC_REG_R1 /* rare */

#define PPC_REG_R2  2
/* ... */
#define PPC_REG_R31 31

#define PPC_REG_PC  32

#define PPC_REG_LR (-1)


//***************************************************************************
// Operands
//***************************************************************************

#define PPC_OPND_REG_S_MASK  0x03e00000
#define PPC_OPND_REG_S_SHIFT 21
#define PPC_OPND_REG_S(insn) \
  (((insn) & PPC_OPND_REG_S_MASK) >> PPC_OPND_REG_S_SHIFT)

#define PPC_OPND_REG_T_MASK  PPC_OPND_REG_S_MASK
#define PPC_OPND_REG_T_SHIFT PPC_OPND_REG_S_SHIFT
#define PPC_OPND_REG_T(insn) PPC_OPND_REG_S(insn)

#define PPC_OPND_REG_A_MASK  0x001f0000
#define PPC_OPND_REG_A_SHIFT 16
#define PPC_OPND_REG_A(insn) \
  (((insn) & PPC_OPND_REG_A_MASK) >> PPC_OPND_REG_A_SHIFT)

#define PPC_OPND_REG_B_MASK  0x0000f800
#define PPC_OPND_REG_B_SHIFT 11
#define PPC_OPND_REG_B(insn) \
  (((insn) & PPC_OPND_REG_B_MASK) >> PPC_OPND_REG_B_SHIFT)

#define PPC_OPND_CC_MASK     0x00000001  /* Condition code bit */
#define PPC_OPND_CC_SHIFT    0

#define PPC_OPND_DISP(x)     (((int16_t)((x) & 0x0000ffff)))



//***************************************************************************
// Opcodes
//***************************************************************************

#define PPC_OP_D_MASK 0xfc000000  /* opcode */
#define PPC_OP_X_MASK 0xfc0007fe  /* opcode, extra-opc */

#define PPC_OP_LWZ    0x80000000 /* D-form */
#define PPC_OP_STW    0x90000000 /* D-form */
#define PPC_OP_STWU   0x94000000 /* D-form */

#define PPC_OP_ADDI   0x38000000 /* D-form */
#define PPC_OP_ADDIS  0x3c000000 /* D-form */ 
#define PPC_OP_LIS    PPC_OP_ADDIS /* D-form: lis Rx,v = addis Rx,0,v */


#define PPC_OP_STWUX  0x7c00016e /* X-form */

#define PPC_OP_OR     0x7c000378 /* X-form */
#define PPC_OP_MR     PPC_OP_OR  /* X-form: mr Rx Ry = or Rx Ry Ry */


//***************************************************************************
// Instructions
//***************************************************************************

#define PPC_INSN_D_MASK 0xffff0000 /* opcode RS, RA */
#define PPC_INSN_X_MASK 0xfffffffe /* opcode RS, RA, RB, extra-opc */

#define PPC_INSN_D(opc, RS, RA, D) \
  ((opc) | ((RS) << PPC_OPND_REG_S_SHIFT) \
         | ((RA) << PPC_OPND_REG_A_SHIFT) \
         | (D))

#define PPC_INSN_X(opc, RS, RA, RB, Rc) \
  ((opc) | ((RS) << PPC_OPND_REG_S_SHIFT) \
         | ((RA) << PPC_OPND_REG_A_SHIFT) \
         | ((RB) << PPC_OPND_REG_B_SHIFT) \
         | (Rc))

/****************************************************************************/

#endif /* isa_instructionset_ppc_h */
