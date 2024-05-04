// SPDX-FileCopyrightText: 2011-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SPECIFIC_UCONTEXT_MANIP
#define SPECIFIC_UCONTEXT_MANIP

#include <ucontext.h>
#include "instruction-set.h"

#if __WORDSIZE == 32
#  define UC_REGS(uc, reg) (uc.uc_mcontext.uc_regs->gregs[reg])
#else
#  define UC_REGS(uc, reg) (uc.uc_mcontext.gp_regs[reg])
#endif

#define UC_REG_IP(uc) UC_REGS(uc, PPC_REG_PC)
#define UC_REG_FP(uc) UC_REGS(uc, PPC_REG_FP)
#define UC_REG_SP(uc) UC_REGS(uc, PPC_REG_SP)

#endif // SPECIFIC_UCONTEXT_MANIP
