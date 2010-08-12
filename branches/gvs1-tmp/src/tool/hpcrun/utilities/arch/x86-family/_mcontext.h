#ifndef __MCONTEXT_H
#define __MCONTEXT_H

#include <ucontext.h>
#include <sys/ucontext.h>

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// various architectures
//-------------------------------------------------------------------------

#ifdef REG_RIP
#define REG_INST_PTR REG_RIP
#define REG_BASE_PTR REG_RBP
#define REG_STACK_PTR REG_RSP
#else

#ifdef REG_EIP
#define REG_INST_PTR REG_EIP
#define REG_BASE_PTR REG_EBP
#define REG_STACK_PTR REG_ESP
#else 
#ifdef REG_IP
#define REG_INST_PTR REG_IP
#define REG_BASE_PTR REG_BP
#define REG_STACK_PTR REG_SP
#endif
#endif
#endif


#define MCONTEXT_REG(mctxt, reg) (mctxt->gregs[reg])
#define MCONTEXT_PC(mctxt) ((void *)  MCONTEXT_REG(mctxt, REG_INST_PTR))
#define MCONTEXT_BP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_BASE_PTR))
#define MCONTEXT_SP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_STACK_PTR))

#endif // __MCONTEXT_H
