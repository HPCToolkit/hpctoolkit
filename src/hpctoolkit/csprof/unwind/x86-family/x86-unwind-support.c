#include <ucontext.h>

#include "x86-decoder.h"
#include "unwind.h"

void
unw_init_arch(void)
{
  x86_family_decoder_init();
}


void 
unw_init_cursor_arch(ucontext_t* context, unw_cursor_t *cursor)
{
  mcontext_t * mctxt = &(context->uc_mcontext);

#ifdef __CRAYXT_CATAMOUNT_TARGET
  cursor->pc = (void *)  mctxt->sc_rip;
  cursor->bp = (void **) mctxt->sc_rbp;
  cursor->sp = (void **) mctxt->sc_rsp;
#else
  cursor->pc = (void *)  mctxt->gregs[REG_RIP];
  cursor->bp = (void **) mctxt->gregs[REG_RBP];
  cursor->sp = (void **) mctxt->gregs[REG_RSP];
#endif
}


int 
unw_get_reg_arch(unw_cursor_t *cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;

  return 0;
}
