// NOTE: For static linked versions, this MUST BE compiled for
//       the target system, as the include file that defines the
//       structure for the contexts may be different for the
//       target system vs the build system.
// !! ESPECIALLY FOR CATAMOUNT !!
//
#include <ucontext.h>

// This unwinder presumes that there are stack frames
#include "prim_unw_cursor.h"

void unw_init_f_context(void *context,unw_cursor_t *frame){

  ucontext_t *ctx = (ucontext_t *) context;

  frame->pc = (void *)ctx->uc_mcontext.sc_rip;
  frame->bp = (void **)ctx->uc_mcontext.sc_rbp;
}

// This get reg just extracts the pc, regardless of REGID

int unw_get_reg(unw_cursor_t *cursor,int REGID,void **regv){
  *regv = cursor->pc;
  return 0;
}

int unw_step (unw_cursor_t *cursor){
  void **bp;
  void *rip;

  bp         = cursor->bp;
  cursor->pc = bp[1];
  cursor->bp = (void **) bp[0];

  return 1;
}
