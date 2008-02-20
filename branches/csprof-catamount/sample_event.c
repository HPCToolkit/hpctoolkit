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

//*************************** Forward Declarations **************************

void csprof_set_handling_sample(thread_data_t *td, int in);

static void
csprof_take_profile_sample(csprof_state_t *state, struct ucontext *ctx,
			   int metric_id, size_t sample_count);

//***************************************************************************

// routine to take samples
// common sampling code, called by all event handlers

int samples_taken    = 0;
int bad_unwind_count = 0;

//FIXME: Add the pc argument ???
void 
csprof_sample_event(void *context, int metric_id, size_t sample_count)
{
  sigjmp_buf_t *it = get_bad_unwind();
  samples_taken++;

  PMSG(SAMPLE,"Handling sample");

  thread_data_t *td = (thread_data_t *) pthread_getspecific(my_thread_specific_key);

  csprof_set_handling_sample(td, 1);

  // FIXME: setup_segv only necessary here because some apps install segv handler of their own
  // setup_segv();

  csprof_state_t *state = csprof_get_state();

  if (!sigsetjmp(it->jb,1)){

    MPI_SPECIAL_SKIP();

    struct ucontext *ctx = (struct ucontext *)(context);
    if(state != NULL) {
      csprof_take_profile_sample(state, ctx, metric_id, sample_count);
      csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
    }
  }
  else {
    EMSG("got bad unwind: context_pc = %p, unwind_pc = %p\n\n",state->context_pc,
         state->unwind_pc);
    dump_backtraces(state, state->unwind);
    bad_unwind_count++;
  }

  csprof_set_handling_sample(td, 0);
}


static void
csprof_take_profile_sample(csprof_state_t *state, struct ucontext *ctx,
			   int metric_id, size_t sample_count)
{
  int ret;

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
  
  ret = csprof_sample_callstack(state, ctx, metric_id, sample_count);
  if (ret == CSPROF_OK) {
#ifdef USE_TRAMP
    PMSG(SWIZZLE,"about to swizzle w context\n");
    csprof_swizzle_with_context(state, (void *)context);
#endif
  }

  csprof_state_flag_clear(state, (CSPROF_TAIL_CALL 
				  | CSPROF_EPILOGUE_RA_RELOADED 
				  | CSPROF_EPILOGUE_SP_RESET));
}



void csprof_set_handling_sample(thread_data_t *td, int in)
{
  td->handling_sample = in;
}

int
csprof_is_handling_sample(void)
{
  thread_data_t *td = (thread_data_t *) pthread_getspecific(my_thread_specific_key);
  return td->handling_sample;
}
