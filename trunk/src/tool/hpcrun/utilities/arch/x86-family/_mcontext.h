#ifndef __MCONTEXT_H
#define __MCONTEXT_H

#include <ucontext.h>

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// various architectures
//-------------------------------------------------------------------------

#define MCONTEXT_REG(mctxt, reg) (mctxt->gregs[reg])
#define MCONTEXT_PC(mctxt) ((void *)  MCONTEXT_REG(mctxt, REG_RIP))
#define MCONTEXT_BP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RBP))
#define MCONTEXT_SP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RSP))

#endif // __MCONTEXT_H
