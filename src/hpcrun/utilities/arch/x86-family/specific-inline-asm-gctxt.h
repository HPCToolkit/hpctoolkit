// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

//***************************************************************************
// local include files
//***************************************************************************

#include "../ucontext-manip.h"



//***************************************************************************
// macros
//***************************************************************************

// macro: INLINE_ASM_GCTXT
// purpose:
//   lightweight version of getcontext, which saves
//   PC, BP, SP into a ucontext structure in their
//   normal positions

#define INLINE_ASM_GCTXT(uc)                               \
  /* load PC into RAX; save RAX into uc structure   */     \
  asm volatile ("leaq 0(%%rip), %%rax\n\t"                 \
                "movq %%rax, %0"                           \
                : "=m" (UC_REG_IP(uc)) /* output    */     \
                :                      /* no inputs */     \
                : "%rax"               /* clobbered */ );  \
  /* save BP into uc structure                      */     \
  asm volatile ("movq %%rbp, %0"                           \
                : "=m" (UC_REG_BP(uc)) /* output    */ );  \
  /* save SP into uc structure                      */     \
  asm volatile ("movq %%rsp, %0"                           \
                : "=m" (UC_REG_SP(uc)) /* output    */ )



#endif // SPECIFIC_INLINE_ASM_GCTXT
