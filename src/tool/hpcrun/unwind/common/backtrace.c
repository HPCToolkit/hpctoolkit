// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *


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

#include <hpcrun/state.h>
#include <monitor.h>
#include <hpcrun/sample_event.h>

#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <trampoline/common/trampoline.h>

//***************************************************************************
// forward declarations 
//***************************************************************************

static csprof_cct_node_t*
_hpcrun_backtrace(csprof_state_t* state, ucontext_t* context,
		  int metricId, uint64_t metricIncr,
		  int skipInner);


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
  if (hpcrun_isLogicalUnwind()) {
    n = lush_backtrace(state, context, metricId, metricIncr, skipInner, 
		       isSync);
  }
  else {
    n = _hpcrun_backtrace(state, context, metricId, metricIncr, skipInner);
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
hpcrun_filter_sample(int len, hpcrun_frame_t *start, hpcrun_frame_t *last)
{
  return ( !(monitor_in_start_func_narrow(last->ip) && (len > 1)) );
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

  //
  // FIXME-TRAMP: cannot reset the backtrace buffer to beginning each time
  // if a tramp has been set.
  //   OR ELSE: figure out something else to do about cached backtrace...
  //
  state->unwind   = state->btbuf; // innermost
  state->bufstk   = state->bufend;
  state->treenode = NULL;

  int unw_len = 0;
  while (true) {
    int ret;

    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    if (hpcrun_trampoline_interior(ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline
      unw_throw();
    }
    if (hpcrun_trampoline_at_entry(ip)) {
      if (unw_len == 0){
	//
	// Possibly change later to do something more sophisticated ...
	//
	unw_throw();
      }
      else {
	// *** maybe put a break here, as backtrace is now done ***
	// put cached backtrace prefix + current unwind into cct
	// remove current trampoline
	// insert trampoline to new location
      }
    }
    
    csprof_state_ensure_buffer_avail(state, state->unwind);

    state->unwind->cursor = cursor;
    state->unwind->ip     = (void *) ip;
    state->unwind->ra_loc = NULL;
    hpcrun_frame_t* prev = state->unwind;
    state->unwind++;
    unw_len++;

    state->unwind_pc = (void *) ip; // mark starting point in case of failure

    ret = unw_step(&cursor);
    backtrace_trolled = (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }
  if (backtrace_trolled){
    csprof_up_pmsg_count();
  }
  
  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    if (hpcrun_filter_sample(unw_len, state->btbuf, state->unwind - 1)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", unw_len);
      hpcrun_frame_t *fr = state->btbuf;
      for (int i = 0; i < unw_len; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p",i,fr->ip);
      }
      csprof_inc_samples_filtered();
      return 0;
    }
  }

  hpcrun_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  hpcrun_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  if (skipInner) {
    bt_beg = hpcrun_skip_chords(bt_end, bt_beg, skipInner);
  }

  csprof_cct_node_t* n;
  n = csprof_state_insert_backtrace(state, metricId,
				    bt_end, bt_beg,
				    (cct_metric_data_t){.i = metricIncr});
  return n;
}


//***************************************************************************
// 
//***************************************************************************

hpcrun_frame_t*
hpcrun_skip_chords(hpcrun_frame_t* bt_outer, hpcrun_frame_t* bt_inner, 
		   int skip)
{
  // N.B.: INVARIANT: bt_inner < bt_outer
  hpcrun_frame_t* x_inner = bt_inner;
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
dump_backtrace(csprof_state_t *state, hpcrun_frame_t* unwind)
{
  static const int msg_limit = 100;
  int msg_cnt = 0;

  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  PMSG_LIMIT(EMSG("-- begin new backtrace (innermost first) ------------"));

  if (unwind) {
    for (hpcrun_frame_t* x = state->btbuf; x < unwind; ++x) {
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
    for (hpcrun_frame_t* x = state->bufstk; x < state->bufend; ++x) {
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

