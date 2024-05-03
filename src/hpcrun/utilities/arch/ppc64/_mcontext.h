// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __MCONTEXT_H
#define __MCONTEXT_H

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>

#include "instruction-set.h"


#if __WORDSIZE == 32
#  define UCONTEXT_REG(uc, reg) ((uc)->uc_mcontext.uc_regs->gregs[reg])
#else
#  define UCONTEXT_REG(uc, reg) ((uc)->uc_mcontext.gp_regs[reg])
#endif

//***************************************************************************
// private operations
//***************************************************************************

static inline void *
ucontext_pc(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_PC);
}


static inline void **
ucontext_fp(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_FP);
}


static inline void **
ucontext_sp(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_SP);
}


//***************************************************************************

static inline void**
getNxtPCLocFromSP(void** sp)
{
#ifdef __PPC64__
  static const int RA_OFFSET_FROM_SP = 2;
#else
  static const int RA_OFFSET_FROM_SP = 1;
#endif
  return sp + RA_OFFSET_FROM_SP;
}

static inline void*
getNxtPCFromSP(void** sp)
{
#ifdef __PPC64__
  static const int RA_OFFSET_FROM_SP = 2;
#else
  static const int RA_OFFSET_FROM_SP = 1;
#endif
  return *(sp + RA_OFFSET_FROM_SP);
}


static inline bool
isPossibleParentSP(void** sp, void** parent_sp)
{
  // Stacks grow down, so outer frames are at higher addresses
  return (parent_sp > sp); // assume frame size is not 0
}


#endif // __MCONTEXT_H
