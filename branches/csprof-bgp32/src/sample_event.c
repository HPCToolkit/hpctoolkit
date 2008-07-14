// -*-Mode: C++;-*- // technically C99
// $Id: sample_event.c 1474 2008-06-23 15:13:35Z johnmc $

//*************************** User Include Files ****************************

#include "dump_backtraces.h"
#include "getregs.h"
#include "metrics_types.h"
#include "pmsg.h"
#include "segv_handler.h"
#include "splay.h"
#include "state.h"
#include "thread_data.h"
#include "thread_use.h"
#include "trace.h"
#include "backtrace.h"
#include "csprof_csdata.h"
#include "handling_sample.h"
#include "fnbounds_interface.h"
#include "unwind.h"



//*************************** Forward Declarations **************************

static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, void *context,
			   int metric_id, size_t sample_count);

//***************************************************************************

// routine to take samples
// common sampling code, called by all event handlers

int samples_taken    = 0;
int bad_unwind_count = 0;
int filtered_samples = 0; // global variable to count filtered samples

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, size_t sample_count)
{
  TMSG(SAMPLE,"Handling sample");

  thread_data_t *td = csprof_get_thread_data();
  sigjmp_buf_t *it = &(td->bad_unwind);

  samples_taken++;

#if 0 // just to check on getting itimer samples

#include <stdlib.h>

  if (samples_taken > 5) {
    _exit(0);
  }
  // return;
#endif
  csprof_set_handling_sample(td);

  csprof_cct_node_t* node = NULL;
  csprof_state_t *state = td->state;

  if (!sigsetjmp(it->jb,1)){

    if (state != NULL) {
      node = csprof_take_profile_sample(state, context, metric_id, sample_count);

      if (trace_isactive()) {
	void *pc = context_pc(context);
	csprof_cct_t *cct = &(td->state->csdata); 
	void *func_start_pc, *func_end_pc;

	fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc); 

	csprof_frame_t frm = {.ip = func_start_pc};
	csprof_cct_node_t* func_proxy = csprof_cct_get_child(cct, node->parent, &frm);

	// assign it a call path node id if it doesn't have one
	if (func_proxy->cpid == 0) func_proxy->cpid = cct->next_cpid++;
	trace_append(func_proxy->cpid);
      }
      csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
    }
  }
  else {
    EMSG("got bad unwind: context_pc = %p, unwind_pc = %p\n\n",state->context_pc,
         state->unwind_pc);
    dump_backtraces(state, state->unwind);
    bad_unwind_count++;
    if (TD_GET(splay_lock)){
      csprof_release_splay_lock();
    }
  }

  csprof_clear_handling_sample(td);

  return node;
}


static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, void *context,
			   int metric_id, size_t sample_count)
{
  
  void *pc = context_pc(context);

  PMSG(SAMPLE,"csprof take profile sample");
#ifdef USE_TRAMP
  if(/* trampoline isn't exactly active during exception handling */
     csprof_state_flag_isset(state, CSPROF_EXC_HANDLING)
     /* dynamical libraries are in flux; don't bother */
     /* || csprof_epoch_is_locked() */
     /* general checking for addresses in libraries */
     || csprof_context_is_unsafe(context)) {
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
  n = csprof_sample_callstack(state, context, metric_id, sample_count);

  // FIXME: n == -1 if sample is filtered
#if 0
  if (!n) {
#ifdef USE_TRAMP
    PMSG(SWIZZLE,"about to swizzle w context\n");
    csprof_swizzle_with_context(state, (void *)context);
#endif
  }
#endif

  csprof_state_flag_clear(state, (CSPROF_TAIL_CALL 
				  | CSPROF_EPILOGUE_RA_RELOADED 
				  | CSPROF_EPILOGUE_SP_RESET));
  
  return n;
}


void  
csprof_handling_synchronous_sample(int val) 
{
  thread_data_t *td = csprof_get_thread_data();
  td->handling_synchronous_sample = val;
}


int 
csprof_handling_synchronous_sample_p() 
{
  thread_data_t *td = csprof_get_thread_data();
  return (td->handling_synchronous_sample != 0);
}
