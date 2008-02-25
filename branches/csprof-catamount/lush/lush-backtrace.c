// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <dlfcn.h>

//*************************** User Include Files ****************************

#include "lush.h"

#include <general.h>
#include <state.h>

#include <monitor.h>

//*************************** Forward Declarations **************************

volatile int LUSH_WAIT = 1;

//***************************************************************************
// backtrace
//***************************************************************************

csprof_cct_node_t*
lush_backtrace(csprof_state_t* state, ucontext_t* context,
	       int metric_id, size_t sample_count);


csprof_cct_node_t*
csprof_sample_callstack(csprof_state_t* state, ucontext_t* context,
			int metric_id, size_t sample_count)
{
  csprof_cct_node_t* n;
  n = lush_backtrace(state, context, metric_id, sample_count);
  if (!n) {
    DBGMSG_PUB(1, "LUSH_WAIT... (pid=%d)", getpid()); // FIXME: improve
  }
  return n;
}


csprof_cct_node_t*
lush_backtrace(csprof_state_t* state, ucontext_t* context,
	       int metric_id, size_t sample_count)
{
  if (getenv("LUSH_WAIT")) {
    DBGMSG_PUB(1, "LUSH_WAIT... (pid=%d)", getpid());
    while(LUSH_WAIT);
  }

  lush_cursor_t cursor;
  lush_init_unw(&cursor, state->lush_agents, context);

  // FIXME: processor/x86-64/backtrace.c
  state->unwind   = state->btbuf;
  state->bufstk   = state->bufend;
  state->treenode = NULL;


  // ---------------------------------------------------------
  // Step through bichords
  // ---------------------------------------------------------
  while (lush_step_bichord(&cursor) != LUSH_STEP_END_PROJ) {
    lush_agentid_t aid = lush_cursor_get_aid(&cursor);
    lush_assoc_t as = lush_cursor_get_assoc(&cursor);

    DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Chord: aid:%d assoc:%d", aid, as);
  
    // FIXME: short circuit unwind if we hit the trampoline

    csprof_state_ensure_buffer_avail(state, state->unwind);
    csprof_frame_t* chord_beg = state->unwind; // innermost note
    unsigned int pchord_len = 0, lchord_len = 0;

    // ---------------------------------------------------------
    // Step through p-notes of p-chord
    // ---------------------------------------------------------
    while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
      unw_word_t ip = lush_cursor_get_ip(&cursor);
      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "IP:  %p", ip);
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
      lush_lip_t* lip = lush_cursor_get_lip(&cursor);
      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "LIP: %p", *((void**)lip));
      
      if (lush_assoc_is_x_to_1(as) && !lip_persistent) {
	lip_persistent = lush_lip_clone(lip);
      }
      else {
	lip_persistent = lush_lip_clone(lip);
      }
      state->unwind->lip = lip_persistent;

      lchord_len++;
      state->unwind++;
    }

    // ---------------------------------------------------------
    // Set assoc and fill empty p-notes/l-notes
    // ---------------------------------------------------------
    // INVARIANT: chord_len >= 1
    unsigned int chord_len = MAX(pchord_len, lchord_len); 
    csprof_frame_t* chord_end  = chord_beg + chord_len; // N.B.: exclusive
    csprof_frame_t* pchord_end = chord_beg + pchord_len;
    csprof_frame_t* lchord_end = chord_beg + lchord_len;

    for (csprof_frame_t* x = chord_beg; x < chord_end; ++x) {
      lush_assoc_info__set_assoc(state->unwind->as_info, as);
      lush_assoc_info__set_M(state->unwind->as_info, chord_len);
      
      if (x >= pchord_end) {
	x->ip = NULL;
      }
      if (x >= lchord_end) {
	x->lip = NULL;
      }
    }

    state->unwind = chord_end;
  }

  // ---------------------------------------------------------
  // insert backtrace into calling context tree (FIXME)
  // ---------------------------------------------------------
  csprof_cct_node_t* node = NULL;
#if 0
  node = csprof_state_insert_backtrace(state, metric_id, state->unwind - 1,
				       state->btbuf, sample_count);
#else
  node = (csprof_cct_node_t*)(1); // FIXME: bogus non-NULL value
#endif

  // FIXME: register active marker

  // look at concurrency for agent at top of stack
#if 0 // FIXME
  if (LUSHI_has_concurrency[agent]() ...) {
    lush_agentid_t aid = 1;
    pool->LUSHI_get_concurrency[aid]();
  }
#endif

  return node;
}

