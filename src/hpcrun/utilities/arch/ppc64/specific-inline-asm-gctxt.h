#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

#include "../ucontext-manip.h"

// FIXME: for now, "inline asm" getcontext turns into syscall getcontext
#define INLINE_ASM_GCTXT(uc)  getcontext(&uc)


#endif // SPECIFIC_INLINE_ASM_GCTXT
