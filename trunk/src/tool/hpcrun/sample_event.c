// -*-Mode: C++;-*- // technically C99
// $Id$


//*************************** User Include Files ****************************

#include "atomic-ops.h"
#include "metrics_types.h"
#include "pmsg.h"
#include "segv_handler.h"
#include "state.h"
#include "thread_data.h"
#include "trace.h"
#include "backtrace.h"
#include "csprof_csdata.h"
#include "handling_sample.h"
#include "fnbounds_interface.h"
#include "unwind.h"
#include "csprof-malloc.h"
#include "splay-interval.h"
#include "sample_sources_all.h"
#include "ui_tree.h"
#include "string.h"

//*************************** Forward Declarations **************************

static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, void *context,
			   int metric_id, unsigned long long metric_units_consumed);

//***************************************************************************

// routine to take samples
// common sampling code, called by all event handlers

int samples_taken    = 0;
int bad_unwind_count = 0;
int filtered_samples = 0; // global variable to count filtered samples

static int _sampling_disabled = 0;

static long num_samples_blocked_async = 0;

void
csprof_inc_samples_blocked_async(void)
{
  fetch_and_add(&num_samples_blocked_async, 1L);
}

long
csprof_num_samples_blocked_async(void)
{
  return num_samples_blocked_async;
}

int
sampling_is_disabled(void)
{
  return _sampling_disabled;
}

void
csprof_disable_sampling(void)
{
  TMSG(SPECIAL,"Sampling disabled");
  _sampling_disabled = 1;
}

void
csprof_drop_sample(void)
{
  TMSG(DROP, "dropping sample");
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  siglongjmp(it->jb,9);
}

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, unsigned long long metric_units_consumed)
{
  samples_taken++;

  TMSG(SAMPLE,"Handling sample");
  if (_sampling_disabled){
    TMSG(SAMPLE,"global suspension");
    csprof_all_sources_stop();
    //    csprof_all_sources_hard_stop();
    return NULL;
  }

  thread_data_t *td = csprof_get_thread_data();
  sigjmp_buf_t *it = &(td->bad_unwind);
  csprof_cct_node_t* node = NULL;
  csprof_state_t *state = td->state;

  csprof_set_handling_sample(td);

  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {

    if (state != NULL) {
      node = csprof_take_profile_sample(state, context, metric_id, metric_units_consumed);

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
    // ------------------------------------------------------------
    // recover from SEGVs and dropped samples
    // ------------------------------------------------------------
    memset((void *)it->jb, '\0', sizeof(it->jb));
    dump_backtrace(state, state->unwind);
    bad_unwind_count++;
    csprof_up_pmsg_count();
    if (TD_GET(splay_lock)) {
      csprof_release_splay_lock();
    }
    if (TD_GET(fnbounds_lock)) {
      fnbounds_release_lock();
    }
  }

  csprof_clear_handling_sample(td);

  return node;
}

static csprof_cct_node_t*
csprof_take_profile_sample(csprof_state_t *state, void *context,
			   int metric_id, unsigned long long metric_units_consumed)
{
  void *pc = context_pc(context);

  TMSG(SAMPLE,"csprof take profile sample");
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
  TMSG(SAMPLE, "Signalled at %#lx", pc);

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
  n = csprof_sample_callstack(state, context, metric_id, metric_units_consumed);

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
