#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

#include <utilities/arch/ucontext-manip.h>

#define icat(pre, base) pre ## base
#define lcl_label(txt) icat(local_, txt)
#define INLINE_ASM_GCTXT_IP(LABEL, uc)       \
  extern void lcl_label(LABEL);              \
  asm volatile (".globl " "local_" #LABEL);  \
  asm volatile ("local_" #LABEL ":");        \
  UC_REG_IP(uc) = (greg_t) &lcl_label(LABEL)

#define INLINE_ASM_GCTXT(LABEL, uc)          \
  INLINE_ASM_GCTXT_IP(LABEL, uc);            \
  asm volatile ("movq %%rbp, %0"             \
                : "=m" (UC_REG_BP(uc)));     \
  asm volatile ("movq %%rsp, %0"             \
                : "=m" (UC_REG_SP(uc)))

#endif // SPECIFIC_INLINE_ASM_GCTXT
