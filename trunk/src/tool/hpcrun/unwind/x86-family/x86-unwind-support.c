// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include "x86-decoder.h"
#include "unwind.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// macros
//***************************************************************************

#if defined(__LIBCATAMOUNT__)
#undef __CRAYXT_CATAMOUNT_TARGET
#define __CRAYXT_CATAMOUNT_TARGET
#endif

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// Linux and Catamount
//-------------------------------------------------------------------------
#ifdef __CRAYXT_CATAMOUNT_TARGET

#define MCONTEXT_PC(mctxt) ((void *)   mctxt->sc_rip)
#define MCONTEXT_BP(mctxt) ((void **)  mctxt->sc_rbp)
#define MCONTEXT_SP(mctxt) ((void **)  mctxt->sc_rsp)

#else

#define MCONTEXT_REG(mctxt, reg) (mctxt->gregs[reg])
#define MCONTEXT_PC(mctxt) ((void *)  MCONTEXT_REG(mctxt, REG_RIP))
#define MCONTEXT_BP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RBP))
#define MCONTEXT_SP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RSP))

#endif



//***************************************************************************
// interface functions
//***************************************************************************

void
unw_init_arch(void)
{
  x86_family_decoder_init();
}


void *
context_pc(void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);

  return MCONTEXT_PC(mc);
}


void 
unw_init_cursor_arch(void* context, unw_cursor_t* cursor)
{
  mcontext_t *mc = GET_MCONTEXT(context);

  cursor->pc = MCONTEXT_PC(mc); 
  cursor->bp = MCONTEXT_BP(mc);
  cursor->sp = MCONTEXT_SP(mc);
}


int 
unw_get_reg_arch(unw_cursor_t *cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;

  return 0;
}
