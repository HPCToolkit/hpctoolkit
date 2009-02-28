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


#define PPC_OP_MFLR_R0(x)    ((x) == 0x7c0802a6)
#define PPC_OP_STW_R0_R1(x)  (((x) & 0xffff0000) == 0x90010000)
#define PPC_OP_STWU_R1_R1(x) (((x) & 0xffff0000) == 0x94210000)
#define PPC_OP_MTLR_R0(x)    ((x) == 0x7c0803a6)
#define PPC_OP_LWZ_R0_R1(x)  (((x) & 0xffff0000) == 0x80010000)
#define PPC_OP_ADDI_R1(x)    (((x) & 0xffff0000) == 0x38210000)
#define PPC_OP_MR_R1(x)      (((x) & 0xffc003ff) == 0x7d400378)
#define PPC_OP_BLR(x)        ((x) == 0x4e800020)

//****************************************************************************


#define PPC_OPND_DISP(x)     (((int16_t)((x) & 0x0000ffff)))

//****************************************************************************

#define PPC_REG_R0 0
#define PPC_REG_SP 1 // typically, R1 is a stack pointer, not a frame pointer
#define PPC_REG_FP 1 

#define PPC_REG_PC 32

#define PPC_REG_LR (-1)

/****************************************************************************/

#endif /* isa_instructionset_ppc_h */
