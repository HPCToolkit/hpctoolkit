#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

// inline getcontext unknown for ia64 platform

#define INLINE_ASM_GCTXT(LABEL, uc)  getcontext(&uc)

#endif // SPECIFIC_INLINE_ASM_GCTXT
