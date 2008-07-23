// -*-Mode: C++;-*- // technically C99
// $Id: backtrace.c 1445 2008-06-14 18:58:36Z johnmc $


//***************************************************************************
// system include files
//***************************************************************************

#include <sys/types.h>
#include <ucontext.h>



//***************************************************************************
// local include files
//***************************************************************************

#include "unwind.h"

#include "backtrace.h"
#include "state.h"
#include "general.h"
#include "dump_backtraces.h"
#include "pmsg.h"
#include "monitor.h"
#include "sample_event.h"



//***************************************************************************
// forward declarations 
//***************************************************************************

#ifdef DEBUG
#define debug_dump_backtraces(x,y) dump_backtraces(x,y)
#else
#define debug_dump_backtraces(x,y)
#endif


static csprof_cct_node_t*
csprof_sample_callstack_from_frame(csprof_state_t *, int,
				   size_t, unw_cursor_t *);



//***************************************************************************
// interface functions
//***************************************************************************

//-----------------------------------------------------------------------------
// function: csprof_sample_callstack
// purpose:
//     if successful, returns the leaf node representing the sample;
//     otherwise, returns NULL.
//-----------------------------------------------------------------------------
csprof_cct_node_t*
csprof_sample_callstack(csprof_state_t *state, ucontext_t* context, 
			int metric_id, size_t sample_count)
{
  unw_cursor_t frame;

  csprof_state_verify_backtrace_invariants(state);

  char *s = context ? "context NOT null" : "context IS null";
  // TMSG(GETCONTEXT,s);
  unw_init_cursor(context, &frame);
  TMSG(GETCONTEXT,"back from cursor init: pc = %p, bp = %p",frame.pc,frame.bp);

  csprof_cct_node_t* n;
  n = csprof_sample_callstack_from_frame(state, metric_id,
					 sample_count, &frame);

  return n;
}



//***************************************************************************
// private operations 
//***************************************************************************

//---------------------------------------------------------------------------
// function: csprof_sample_filter
//
// purpose:
//     ignore any samples that aren't rooted at designated call sites in 
//     monitor that should be at the base of all process and thread call 
//     stacks. 
//
// implementation notes:
//     to support this, in monitor we define a pair of labels surrounding the 
//     call site of interest. it is possible to get a sample between the pair 
//     of labels that is outside the call. in that case, the length of the 
//     sample's callstack would be 1, and we ignore it.
//-----------------------------------------------------------------------------
static int
csprof_sample_filter(int len, csprof_frame_t *start, csprof_frame_t *last)
{
  return (! ( monitor_in_start_func_narrow(last->ip) && (len > 1) ));
}


static csprof_cct_node_t*
csprof_sample_callstack_from_frame(csprof_state_t *state, int metric_id,
				   size_t sample_count, unw_cursor_t *cursor)
{
  unw_word_t ip;
  int ret;

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------
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
#if 0
      TMSG(SAMPLE_FILTER,"filter sample of length %d",unw_len);
      csprof_frame_t *fr = state->btbuf;
      for (int i = 0; i < unw_len; i++,fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p",i,fr->ip);
      }
#else
      EMSG("filter sample of length %d shown below", unw_len);
#endif
      dump_backtraces(state, state->unwind);
      filtered_samples++;
      return 0;
    }
  }

  csprof_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  csprof_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  csprof_cct_node_t* n;
  n = csprof_state_insert_backtrace(state, metric_id,
				    bt_end, bt_beg,
				    (cct_metric_data_t){.i = sample_count});
  return n;
}
