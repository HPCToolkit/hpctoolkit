// -*-Mode: C++;-*- // technically C99
// $Id$
/* x86-64_backtrace.c -- unwinding the stack on x86-64 w/ frame pointers */

//************************* System Include Files ****************************

#include <sys/types.h>
#include <ucontext.h>

/* FIXME: configure backtrace to use libunwind if that is wanted
   FORNOW: just use binary analyzer
#ifndef PRIM_UNWIND
#include <libunwind.h>
#else
#include "prim_unw.h"
#endif
*/

//*************************** User Include Files ****************************

#include "prim_unw.h"

#include "backtrace.h"
#include "state.h"
#include "general.h"
#include "util.h"
#include "dump_backtraces.h"
#include "pmsg.h"
#include "monitor.h"
#include "sample_event.h"

//*************************** Forward Declarations **************************

#define USE_LIBUNWIND_TO_START 0

extern void _start();

#if 1
#define debug_dump_backtraces(x,y)
#else
#define debug_dump_backtraces(x,y) dump_backtraces(x,y)
#endif


static csprof_cct_node_t*
csprof_sample_callstack_from_frame(csprof_state_t *, int,
				   size_t, unw_cursor_t *);

//***************************************************************************




/* FIXME: some of the checks from libunwind calls are checked for errors.
   I suppose that's a good thing, but there should be some way (CSPROF_PERF?)
   to turn off the checking to avoid branch prediction penalties...

   Then again, branch prediction penalities after you've already made a
   function call and decoded DWARF unwind info and written several words
   of memory...maybe we don't care that much. */

// If successful, returns the leaf node representing the sample;
// otherwise, returns NULL.
csprof_cct_node_t*
csprof_sample_callstack(csprof_state_t *state, ucontext_t* context, 
			int metric_id, size_t sample_count)
{
  mcontext_t* mctxt = &context->uc_mcontext;

#if 0
  int first_ever_unwind = (state->bufstk == state->bufend);
  void *sp1 = first_ever_unwind ? (void *) -1 : state->bufstk->sp;
#endif

#if 0
#ifndef PRIM_UNWIND
  unw_context_t unwctxt;
#endif
#endif

  unw_cursor_t frame;

  csprof_state_verify_backtrace_invariants(state);

#if 0
#if USE_LIBUNWIND_TO_START
  /* would be nice if we could just copy in the signal context... */
  if(unw_getcontext(&unwctxt) < 0) {
    DIE("Could not initialize unwind context!", __FILE__, __LINE__);
  }
#else
#ifndef PRIM_UNWIND
  memcpy(&unwctxt.uc_mcontext, mctxt, sizeof(mcontext_t));
#else
  unw_init_mcontext(mctxt,&frame);
  MSG(1,"back from cursor init: pc = %p, bp = %p\n",frame.pc,frame.bp);
#endif
#endif
#endif

  unw_init_mcontext(mctxt,&frame);
  MSG(1,"back from cursor init: pc = %p, bp = %p\n",frame.pc,frame.bp);

#if 0
#ifndef PRIM_UNWIND
  if(unw_init_local(&frame, &unwctxt) < 0) {
    DIE("Could not initialize unwind cursor!", __FILE__, __LINE__);
  }
#endif
#if USE_LIBUNWIND_TO_START
  {
    int i;

    /* unwind through libcsprof and the signal handler */
    for(i=0; i<CSPROF_SAMPLE_CALLSTACK_DEPTH; ++i) {
      if(unw_step(&frame) < 0) {
	DIE("An error occurred while unwinding", __FILE__, __LINE__);
      }
    }
  }
#endif
#endif

  csprof_cct_node_t* n;
  n = csprof_sample_callstack_from_frame(state, metric_id,
					 sample_count, &frame);
#if 0
  if (!n){
    ERRMSG("failure recording callstack sample", __FILE__, __LINE__);
  }
#endif
  return n;
}


static int
csprof_sample_filter(int len,csprof_frame_t *start, csprof_frame_t *last)
{
  return (! ( monitor_in_start_func_narrow(last->ip) && (len > 1) ));
}


static csprof_cct_node_t*
csprof_sample_callstack_from_frame(csprof_state_t *state, int metric_id,
				   size_t sample_count, unw_cursor_t *cursor)
{
  unw_word_t ip;
  int first = 1;
  int ret;

  // FIXME: why cannot some of these be local variables
  state->unwind   = state->btbuf;
  state->bufstk   = state->bufend;
  state->treenode = NULL;

  int unw_len = 0;
  for(;;){
    csprof_state_ensure_buffer_avail(state, state->unwind);

    if (unw_get_reg(cursor, UNW_REG_IP, &ip) < 0) {
      MSG(1,"get_reg break");
      break;
    }

    if (first){
      MSG(1,"BTIP = %lp",(void *)ip);
      /*        first = 0; */
    }

    state->unwind_pc = (void *) ip; // mark starting point in case of failure

    state->unwind->ip = (void *) ip;
    state->unwind->sp = (void *) 0;
    state->unwind++;
    unw_len++;

    ret = unw_step(cursor);
    if (ret <= 0) {
      MSG(1,"Hit unw_step break");
      break;
    }
  }
  MSG(1,"BTIP------------");
  debug_dump_backtraces(state,state->unwind);
  
  IF_NOT_DISABLED(SAMPLE_FILTERING){
    if (csprof_sample_filter(unw_len,state->btbuf,state->unwind - 1)){
      TMSG(SAMPLE_FILTER,"filter sample of length %d",unw_len);
      csprof_frame_t *fr = state->btbuf;
      for (int i = 0; i < unw_len; i++,fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p",i,fr->ip);
      }
      filtered_samples++;
      return 0;
    }
  }

  csprof_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  csprof_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  csprof_cct_node_t* n;
  n = csprof_state_insert_backtrace(state, metric_id, 
				    bt_end, bt_beg, sample_count);
  return n;
}
