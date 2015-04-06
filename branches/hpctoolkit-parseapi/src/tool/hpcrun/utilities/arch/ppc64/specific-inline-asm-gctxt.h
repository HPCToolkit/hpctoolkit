#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

#include <utilities/arch/ucontext-manip.h>

//
#if 0 // ppc code unknown, so "inline asm"
      // for ppc platform doesn't work yet

#define icat(pre, base) pre ## base
#define lcl_label(txt) icat(L, txt)

// ****FIXME**** mov instructions need to be worked out for ppc64

#define INLINE_ASM_GCTXT(LABEL, uc)           \
  extern void lcl_label(LABEL);                \
  asm volatile (".globl " "L" #LABEL);         \
  asm volatile ("L" #LABEL ":");               \
  UC_REG_IP(uc) = (greg_t) &lcl_label(LABEL); \
// need equivalent ppc code here
  asm volatile ("movq %%rbp, %0"               \
                : "=m" (UC_REG_BP(uc)));      \
  asm volatile ("movq %%rsp, %0"               \
                : "=m" (UC_REG_SP(uc)))
#else // for now, "inline asm" getcontext turns into syscall getcontext
#define INLINE_ASM_GCTXT(LABEL, uc)  getcontext(&uc)
#endif // ppc code unknown


#endif // SPECIFIC_INLINE_ASM_GCTXT
