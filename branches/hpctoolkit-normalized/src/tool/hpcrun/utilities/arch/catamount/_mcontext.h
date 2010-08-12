#ifndef __MCONTEXT_H
#define __MCONTEXT_H

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// Linux and Catamount
//-------------------------------------------------------------------------

#define MCONTEXT_PC(mctxt) ((void *)   mctxt->sc_rip)
#define MCONTEXT_BP(mctxt) ((void **)  mctxt->sc_rbp)
#define MCONTEXT_SP(mctxt) ((void **)  mctxt->sc_rsp)

#endif // __MCONTEXT_H
