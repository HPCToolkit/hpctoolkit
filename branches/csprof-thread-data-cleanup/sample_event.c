// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>

//*************************** User Include Files ****************************

#include "bad_unwind.h"
#include "dump_backtraces.h"
#include "getregs.h"
#include "metrics_types.h"
#include "mpi_special.h"
#include "pmsg.h"
#include "segv_handler.h"
#include "state.h"
#include "thread_data.h"
#include "thread_use.h"
#include "backtrace.h"
#include "csprof_csdata.h"
#include "handling_sample.h"

//*************************** Forward Declarations **************************

static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, struct ucontext *ctx,
			   int metric_id, size_t sample_count);

//***************************************************************************

// routine to take samples
// common sampling code, called by all event handlers

int samples_taken    = 0;
int bad_unwind_count = 0;

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, size_t sample_count)
{
  PMSG(SAMPLE,"Handling sample");

  thread_data_t *td = csprof_get_thread_data();
  sigjmp_buf_t *it = &(td->bad_unwind);

  samples_taken++;

  csprof_set_handling_sample(td);

  csprof_cct_node_t* node = NULL;
  csprof_state_t *state = td->state;

  if (!sigsetjmp(it->jb,1)){

    struct ucontext *ctx = (struct ucontext *)(context);
    if (state != NULL) {
      node = csprof_take_profile_sample(state, ctx, metric_id, sample_count);
      csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
    }
  }
  else {
    EMSG("got bad unwind: context_pc = %p, unwind_pc = %p\n\n",state->context_pc,
         state->unwind_pc);
    dump_backtraces(state, state->unwind);
    bad_unwind_count++;
  }

  csprof_clear_handling_sample(td);

  return node;
}


static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, struct ucontext *ctx,
			   int metric_id, size_t sample_count)
{
  mcontext_t *context = &ctx->uc_mcontext;
  void *pc = csprof_get_pc(context);

  PMSG(SAMPLE,"csprof take profile sample");
#ifdef USE_TRAMP
  if(/* trampoline isn't exactly active during exception handling */
     csprof_state_flag_isset(state, CSPROF_EXC_HANDLING)
     /* dynamical libraries are in flux; don't bother */
     /* || csprof_epoch_is_locked() */
     /* general checking for addresses in libraries */
     || csprof_context_is_unsafe(ctx)) {
    EMSG("Reached trampoline code !!");
    /* ugh, don't even bother */
    state->trampoline_samples++;
    // _zflags();

    return;
  }
#endif
  state->context_pc = pc;
  PMSG(SAMPLE, "Signalled at %#lx", pc);

  /* check to see if shared library state has changed out from under us */
  state = csprof_check_for_new_epoch(state);

#ifdef USE_TRAMP
  PMSG(SWIZZLE,"undo swizzled data\n");
  csprof_undo_swizzled_data(state, context);
#endif

#if defined(__ia64__) && defined(__linux__) 
  /* force insertion from the root */
  state->treenode = NULL;
  state->bufstk = state->bufend;
#endif
  
  csprof_cct_node_t* n;
  n = csprof_sample_callstack(state, ctx, metric_id, sample_count);

  if (!n) {
#ifdef USE_TRAMP
    PMSG(SWIZZLE,"about to swizzle w context\n");
    csprof_swizzle_with_context(state, (void *)context);
#endif
  }

  csprof_state_flag_clear(state, (CSPROF_TAIL_CALL 
				  | CSPROF_EPILOGUE_RA_RELOADED 
				  | CSPROF_EPILOGUE_SP_RESET));
  
  return n;
}
