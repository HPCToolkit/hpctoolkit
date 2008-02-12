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
#include "monitor.h"
#include "state.h"
#include "general.h"
#include "util.h"
#include "dump_backtraces.h"

//*************************** Forward Declarations **************************

#define USE_LIBUNWIND_TO_START 0

extern void _start();

#if 1
#define debug_dump_backtraces(x,y)
#else
#define debug_dump_backtraces(x,y) dump_backtraces(x,y)
#endif


int csprof_sample_callstack_from_frame(csprof_state_t *, int,
				       size_t, unw_cursor_t *);

//***************************************************************************

// ***FIXME: mmap interaction

#define ensure_state_buffer_slot_available(state,unwind) \
    if (unwind == state->bufend) { \
        unwind = csprof_state_expand_buffer(state, unwind);\
        state->bufstk = state->bufend; \
}


int csprof_check_fence(void *ip){
  return monitor_unwind_process_bottom_frame(ip) || monitor_unwind_thread_bottom_frame(ip);
}


/* FIXME: some of the checks from libunwind calls are checked for errors.
   I suppose that's a good thing, but there should be some way (CSPROF_PERF?)
   to turn off the checking to avoid branch prediction penalties...

   Then again, branch prediction penalities after you've already made a
   function call and decoded DWARF unwind info and written several words
   of memory...maybe we don't care that much. */
int
csprof_sample_callstack(csprof_state_t *state, int metric_id,
			size_t sample_count, void *context)
{
    int first_ever_unwind = (state->bufstk == state->bufend);
    void *sp1 = first_ever_unwind ? (void *) -1 : state->bufstk->sp;

#if 0
#ifndef PRIM_UNWIND
    unw_context_t ctx;
#endif
#endif

    unw_cursor_t frame;

    csprof_state_verify_backtrace_invariants(state);

#if 0
#if USE_LIBUNWIND_TO_START
    /* would be nice if we could just copy in the signal context... */
    if(unw_getcontext(&ctx) < 0) {
        DIE("Could not initialize unwind context!", __FILE__, __LINE__);
    }
#else
#ifndef PRIM_UNWIND
    memcpy(&ctx.uc_mcontext, context, sizeof(mcontext_t));
#else
    unw_init_f_mcontext(context,&frame);
    MSG(1,"back from cursor init: pc = %p, bp = %p\n",frame.pc,frame.bp);
#endif
#endif
#endif

    unw_init_f_mcontext(context,&frame);
    MSG(1,"back from cursor init: pc = %p, bp = %p\n",frame.pc,frame.bp);

#if 0
#ifndef PRIM_UNWIND
    if(unw_init_local(&frame, &ctx) < 0) {
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

    return csprof_sample_callstack_from_frame(state, metric_id,
					      sample_count, &frame);
}


int
csprof_sample_callstack_from_frame(csprof_state_t *state, int metric_id,
				   size_t sample_count, unw_cursor_t *cursor)
{
  unw_word_t ip;
  int first = 1;
  state->unwind = state->btbuf;

  state->bufstk   = state->bufend;
  state->treenode = NULL;

  for(;;){
    ensure_state_buffer_slot_available(state, state->unwind);

    if (unw_get_reg (cursor, UNW_REG_IP, &ip) < 0){
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

    if (csprof_check_fence((void *)ip) || (unw_step (cursor) <= 0)){
      MSG(1,"Hit unw_step break");
      break;
    }

  }
  MSG(1,"BTIP------------");
  debug_dump_backtraces(state,state->unwind);
  if (csprof_state_insert_backtrace(state, metric_id, state->unwind - 1, 
				    state->btbuf, sample_count) != CSPROF_OK) {
    ERRMSG("failure recording callstack sample", __FILE__, __LINE__);
    return CSPROF_ERR;
  }

  return CSPROF_OK;
}

