// -*-Mode: C++;-*- // technically C99
// $Id$


//***************************************************************************
// system include files
//***************************************************************************

#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <ucontext.h>


//***************************************************************************
// local include files
//***************************************************************************

#include "unwind.h"

#include "backtrace.h"
#include "state.h"
#include "pmsg.h"
#include "monitor.h"
#include "sample_event.h"

#include <lush/lush-backtrace.h>

//***************************************************************************
// forward declarations 
//***************************************************************************

static csprof_cct_node_t*
_hpcrun_backtrace(csprof_state_t* state, ucontext_t* context,
		  int metricId, uint64_t metricIncr,
		  int skipInner);


#if (HPC_UNW_LITE)
static int
hpcrun_backtrace_lite(void** buffer, int size, ucontext_t* context);

static int
test_backtrace_lite(ucontext_t* context);
#endif



//***************************************************************************
// interface functions
//***************************************************************************

//-----------------------------------------------------------------------------
// function: hpcrun_backtrace
// purpose:
//     if successful, returns the leaf node representing the sample;
//     otherwise, returns NULL.
//-----------------------------------------------------------------------------
csprof_cct_node_t*
hpcrun_backtrace(csprof_state_t *state, ucontext_t* context, 
		 int metricId, uint64_t metricIncr,
		 int skipInner, int isSync)
{
  csprof_state_verify_backtrace_invariants(state);
  
  csprof_cct_node_t* n = NULL;
  if (!lush_agents) {
    n = _hpcrun_backtrace(state, context, metricId, metricIncr, 
			  skipInner);
  }
  else {
    n = lush_backtrace(state, context, metricId, metricIncr, 
		       skipInner, isSync);
  }
  //HPC_IF_UNW_LITE(test_backtrace_lite(context);)

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}



//***************************************************************************
// private operations 
//***************************************************************************

//---------------------------------------------------------------------------
// function: hpcrun_filter_sample
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
hpcrun_filter_sample(int len, csprof_frame_t *start, csprof_frame_t *last)
{
#if (HPC_UNW_LITE)
  return ( !(len > 1) );
#else
  return ( !(monitor_in_start_func_narrow(last->ip) && (len > 1)) );
#endif
}


static csprof_cct_node_t*
_hpcrun_backtrace(csprof_state_t* state, ucontext_t* context,
		  int metricId, uint64_t metricIncr, 
		  int skipInner)
{
  int backtrace_trolled = 0;

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------
  state->unwind   = state->btbuf; // innermost
  state->bufstk   = state->bufend;
  state->treenode = NULL;

  int unw_len = 0;
  while (true) {
    int ret;

    unw_word_t ip = 0;
    ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
    if (ret < 0) {
      break;
    }

    csprof_state_ensure_buffer_avail(state, state->unwind);

    state->unwind->ip = (void *) ip;
    state->unwind->sp = (void *) 0;
    state->unwind++;
    unw_len++;

    state->unwind_pc = (void *) ip; // mark starting point in case of failure

    ret = unw_step(&cursor);
    backtrace_trolled = (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
  }
  if (backtrace_trolled){
    csprof_up_pmsg_count();
  }
  
  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    if (hpcrun_filter_sample(unw_len, state->btbuf, state->unwind - 1)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", unw_len);
      csprof_frame_t *fr = state->btbuf;
      for (int i = 0; i < unw_len; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p",i,fr->ip);
      }
      csprof_inc_samples_filtered();
      return 0;
    }
  }

  csprof_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  csprof_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  if (skipInner) {
    bt_beg = hpcrun_skip_chords(bt_end, bt_beg, skipInner);
  }

  csprof_cct_node_t* n;
  n = csprof_state_insert_backtrace(state, metricId,
				    bt_end, bt_beg,
				    (cct_metric_data_t){.i = metricIncr});
  return n;
}


#if (HPC_UNW_LITE)
static int 
hpcrun_backtrace_lite(void** buffer, int size, ucontext_t* context)
{
  // special trivial case: size == 0 (N.B.: permit size < 0)
  if (size <= 0) {
    return 0;
  }

  // INVARIANT: 'size' > 0; 'buffer' is non-empty; 'context' is non-NULL
  if ( !(size > 0 && buffer && context) ) {
    return -1; // error
  }

  unw_init();

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  int my_size = 0;
  while (my_size < size) {
    int ret;

    unw_word_t ip = 0;
    ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
    if (ret < 0) { /* ignore error */ }

    buffer[my_size] = ip; // my_size < size
    my_size++;

    ret = unw_step(&cursor);
    if (ret <= 0) {
      // N.B. (ret < 0) indicates an unwind error, which we ignore
      break;
    }
  }
  
  return my_size;
}
#endif


#if (HPC_UNW_LITE)
static int
test_backtrace_lite(ucontext_t* context)
{
  const int bufsz = 100;

  void* buffer[bufsz];
  int sz = hpcrun_backtrace_lite(buffer, bufsz, context);

  for (int i = 0; i < sz; ++i) {
    TMSG(UNW, "backtrace_lite: pc=%p", buffer[i]);
  }

  return sz;
}
#endif


//***************************************************************************
// 
//***************************************************************************

csprof_frame_t*
hpcrun_skip_chords(csprof_frame_t* bt_outer, csprof_frame_t* bt_inner, 
		   int skip)
{
  // N.B.: INVARIANT: bt_inner < bt_outer
  csprof_frame_t* x_inner = bt_inner;
  for (int i = 0; (x_inner < bt_outer && i < skip); ++i) {
    // for now, do not support M chords
    lush_assoc_t as = lush_assoc_info__get_assoc(x_inner->as_info);
    assert(as == LUSH_ASSOC_NULL || as == LUSH_ASSOC_1_to_1 ||
	   as == LUSH_ASSOC_1_to_0);
    
    x_inner++;
  }
  
  return x_inner;
}


void 
dump_backtrace(csprof_state_t *state, csprof_frame_t* unwind)
{
  static const int msg_limit = 100;
  int msg_cnt = 0;

  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  PMSG_LIMIT(EMSG("-- begin new backtrace (innermost first) ------------"));

  if (unwind) {
    for (csprof_frame_t* x = state->btbuf; x < unwind; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG("%s: ip %p | lip %s", as_str, x->ip, lip_str));

      msg_cnt++;
      if (msg_cnt > msg_limit) {
        PMSG_LIMIT(EMSG("!!! message limit !!!"));
        break;
      }
    }
  }

  if (msg_cnt <= msg_limit && state->bufstk != state->bufend) {
    PMSG_LIMIT(EMSG("-- begin cached backtrace ---------------------------"));
    for (csprof_frame_t* x = state->bufstk; x < state->bufend; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG("%s: ip %p | lip %s", as_str, x->ip, lip_str));

      msg_cnt++;
      if (msg_cnt > msg_limit) {
        PMSG_LIMIT(EMSG("!!! message limit !!!"));
        break;
      }
    }
  }

  PMSG_LIMIT(EMSG("-- end backtrace ------------------------------------\n"));
}

