// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <string.h>

//************************ libmonitor Include Files *************************

#include <monitor.h>

//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/min-max.h>

#include "lush.h"
#include "lush-backtrace.h"

#include <state.h>
#include <sample_event.h> // csprof_drop_sample()
#include <unwind/common/backtrace.h> // dump_backtrace()


//*************************** Forward Declarations **************************

#define MYDBG 0

static csprof_frame_t*
canonicalize_chord(csprof_frame_t* chord_beg, lush_assoc_t as,
		   uint pchord_len, uint lchord_len);


//***************************************************************************
// LUSH Agents
//***************************************************************************

lush_agent_pool_t* lush_agents = NULL;


//***************************************************************************
// LUSH backtrace
//***************************************************************************

csprof_cct_node_t*
lush_backtrace(csprof_state_t* state, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       int skipInner, int isSync)
{
  // ---------------------------------------------------------
  // Record backtrace if:
  //   1) a lush agent indicates we should
  //   2) a non-lush-relevant metric
  //   3) a synchronous unwind (may have a lush relevant metric)
  // ---------------------------------------------------------
  bool     doMetric = false;
  uint64_t incrMetric = metricIncr;

  bool     doMetricIdleness = false;
  double   incrMetricIdleness = 0.0;
  lush_agentid_t aidMetricIdleness = lush_agentid_NULL; // list of agents

  if (metricId == lush_agents->metric_time) {
    lush_agentid_t aid = 1; // TODO: multiple agents
    if (lush_agents->LUSHI_do_metric[aid](metricIncr, 
					  &doMetric, &doMetricIdleness,
					  &incrMetric, &incrMetricIdleness)) {
      aidMetricIdleness = aid; // case 1
    }
  }

  if (isSync || metricId != lush_agents->metric_time) {
    doMetric = true; // case 2 and 3
  }

  if (!doMetric) {
    return NULL;
  }


  // ---------------------------------------------------------
  // Perform the backtrace
  // ---------------------------------------------------------

  lush_cursor_t cursor;
  lush_init_unw(&cursor, lush_agents, context);

  // FIXME: unwind/common/backtrace.c
  state->unwind   = state->btbuf;  // innermost
  state->bufstk   = state->bufend;
  state->treenode = NULL;


  // ---------------------------------------------------------
  // Step through bichords
  // ---------------------------------------------------------
  uint unw_len = 0;
  lush_step_t ty = LUSH_STEP_NULL;

  while ( (ty = lush_step_bichord(&cursor)) != LUSH_STEP_END_PROJ 
	  && ty != LUSH_STEP_ERROR ) {
    lush_agentid_t aid = lush_cursor_get_aid(&cursor);
    lush_assoc_t as = lush_cursor_get_assoc(&cursor);

    TMSG(LUNW, "Chord: aid:%d assoc:%d", aid, as);
  
    // FIXME: short circuit unwind if we hit the 'active return'

    csprof_state_ensure_buffer_avail(state, state->unwind);
    csprof_frame_t* chord_beg = state->unwind; // innermost note
    uint pchord_len = 0, lchord_len = 0;

    // ---------------------------------------------------------
    // Step through p-notes of p-chord
    // ---------------------------------------------------------
    while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
      csprof_state_ensure_buffer_avail(state, state->unwind);

      unw_word_t ip = lush_cursor_get_ip(&cursor);
      TMSG(LUNW, "IP:  %p", ip);
      state->unwind->ip = ip;

      pchord_len++;
      state->unwind++;
    }

    state->unwind = chord_beg;

    // ---------------------------------------------------------
    // Step through l-notes of l-chord
    // ---------------------------------------------------------
    lush_lip_t* lip_persistent = NULL;
    while (lush_step_lnote(&cursor) != LUSH_STEP_END_CHORD) {
      csprof_state_ensure_buffer_avail(state, state->unwind);

      lush_lip_t* lip = lush_cursor_get_lip(&cursor); // ephemeral
      TMSG(LUNW, "LIP: %p", *((void**)lip));
      
      if (lush_assoc_is_a_to_1(as)) {
	if (!lip_persistent) {
	  lip_persistent = lush_lip_clone(lip);
	}
	// else: lip_persistent is already set
      }
      else {
	// INVARIANT: as must be 1-to-M
	lip_persistent = lush_lip_clone(lip);
      }
      state->unwind->lip = lip_persistent;

      lchord_len++;
      state->unwind++;
    }

    // ---------------------------------------------------------
    // canonicalize frames to form a chord
    // ---------------------------------------------------------
    csprof_frame_t* chord_end;
    chord_end = canonicalize_chord(chord_beg, as, pchord_len, lchord_len);
    unw_len++;

    state->unwind = chord_end;
  }

  if (ty == LUSH_STEP_ERROR) {
    csprof_drop_sample(); // NULL
  }

#if 0
  // FIXME: cf. csprof_sample_filter in backtrace.c
  if ( !(monitor_in_start_func_narrow(bt_end->ip) && unw_len > 1) ) {
    csprof_inc_samples_filtered();
    return NULL;
  }
#endif

  // ---------------------------------------------------------
  // insert backtrace into calling context tree (if sensible)
  // ---------------------------------------------------------
  if (MYDBG) {
    dump_backtrace(state, state->unwind);
  }

  csprof_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  csprof_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  if (skipInner) {
    bt_beg = hpcrun_skip_chords(bt_end, bt_beg, skipInner);
  }

  csprof_cct_node_t* node = NULL;
  node = csprof_state_insert_backtrace(state, metricId,
				       bt_end, bt_beg,
				       (cct_metric_data_t){.i = incrMetric});

  if (doMetricIdleness) {
    //lush_agentid_t aid = aidMetricIdleness;
    int mid = lush_agents->metric_idleness;
    cct_metric_data_increment(mid, &node->metrics[mid], 
			      (cct_metric_data_t){.r = incrMetricIdleness});
  }

  // FIXME: register active return

  return node;
}


// returns the end of the chord (exclusive)
static csprof_frame_t* 
canonicalize_chord(csprof_frame_t* chord_beg, lush_assoc_t as,
		   uint pchord_len, uint lchord_len)
{
  // Set assoc and fill empty p-notes/l-notes

  // INVARIANT: chord_len >= 1
  //   [chord_beg = innermost ... outermost, chord_end)

  uint chord_len = MAX(pchord_len, lchord_len);
  csprof_frame_t* chord_end  = chord_beg + chord_len; // N.B.: exclusive
  csprof_frame_t* pchord_end = chord_beg + pchord_len;
  csprof_frame_t* lchord_end = chord_beg + lchord_len;

  unw_word_t ip = 0;
  lush_lip_t* lip = NULL;

  if (as == LUSH_ASSOC_1_to_M) {
    ip = chord_beg->ip;
  }
  else if (as == LUSH_ASSOC_M_to_1) {
    lip = chord_beg->lip;
  }
  // else: default is fine for a-to-0 and 1-to-1
  
  uint path_len = chord_len;
  for (csprof_frame_t* x = chord_beg; x < chord_end; ++x, --path_len) {
    lush_assoc_info__set_assoc(x->as_info, as);
    lush_assoc_info__set_path_len(x->as_info, path_len);
    
    if (x >= pchord_end) {
      // INVARIANT: as must be 1-to-M
      x->ip = ip;
    }
    if (x >= lchord_end) {
      // INVARIANT: as is one of: M-to-1, a-to-0
      x->lip = lip;
    }
  }
  
  return chord_end;
}


//***************************************************************************
