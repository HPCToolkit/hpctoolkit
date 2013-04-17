#ifndef SPECIFIC_UCONTEXT_MANIP
#define SPECIFIC_UCONTEXT_MANIP

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <ucontext.h>

#define UC_REGS(uc, reg) (uc).uc_mcontext.gregs[reg]

#define UC_REG_IP(uc) UC_REGS(uc, REG_RIP)
#define UC_REG_BP(uc) UC_REGS(uc, REG_RBP)
#define UC_REG_SP(uc) UC_REGS(uc, REG_RSP)

#endif // SPECIFIC_UCONTEXT_MANIP
