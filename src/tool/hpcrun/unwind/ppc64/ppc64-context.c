#include <ucontext.h>
#include <assert.h>

#include "context.h"

#define PPC64_PC 32
#define PPC64_BP 1
#define PPC64_SP 1 /* wrong */ 

#if __WORDSIZE == 32
#define UCONTEXT_REG(uc, reg) (((ucontext_t *)uc)->uc_mcontext.uc_regs->gregs[reg])
#else
#define UCONTEXT_REG(uc, reg) (((ucontext_t *)uc)->uc_mcontext.gp_regs[reg])
#endif

void *
context_pc(void *context)
{
  return (void *)  UCONTEXT_REG(context, PPC64_PC);
}

void **
context_bp(void *context)
{
  return (void *)  UCONTEXT_REG(context, PPC64_BP);
}


void **
context_sp(void *context)
{
  return (void *)  UCONTEXT_REG(context, PPC64_SP);
}
