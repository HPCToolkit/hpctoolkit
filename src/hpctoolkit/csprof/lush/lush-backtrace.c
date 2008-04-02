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
#include "lush-backtrace.h"

#include <general.h>
#include <state.h>

#include <monitor.h>

//*************************** Forward Declarations **************************

static csprof_frame_t*
canonicalize_chord(csprof_frame_t* chord_beg, lush_assoc_t as,
		   unsigned int pchord_len, unsigned int lchord_len);


//***************************************************************************
// LUSH Agents
//***************************************************************************

// FIXME: def in state.c for the time being
//lush_agent_pool_t* lush_agents = NULL;

//***************************************************************************
// LUSH backtrace
//***************************************************************************


csprof_cct_node_t*
csprof_sample_callstack(csprof_state_t* state, ucontext_t* context,
			int metric_id, size_t sample_count)
{
  csprof_cct_node_t* n;
  n = lush_backtrace(state, context, metric_id, sample_count);
  if (!n) {
    DBGMSG_PUB(1, "LUSH... (pid=%d)", getpid()); // FIXME: improve
  }
  return n;
}


csprof_cct_node_t*
lush_backtrace(csprof_state_t* state, ucontext_t* context,
	       int metric_id, size_t sample_count)
{
  lush_cursor_t cursor;
  lush_init_unw(&cursor, lush_agents, context);

  // FIXME: processor/x86-64/backtrace.c
  state->unwind   = state->btbuf;  // innermost
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
      csprof_state_ensure_buffer_avail(state, state->unwind);

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
      csprof_state_ensure_buffer_avail(state, state->unwind);

      lush_lip_t* lip = lush_cursor_get_lip(&cursor); // ephemeral
      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "LIP: %p", *((void**)lip));
      
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

    state->unwind = chord_end;
  }

  // ---------------------------------------------------------
  // insert backtrace into calling context tree
  // ---------------------------------------------------------
  csprof_frame_t* bt_beg = state->btbuf;      // innermost, inclusive 
  csprof_frame_t* bt_end = state->unwind - 1; // outermost, inclusive

  //dump_backtraces(state, state->unwind);
  csprof_cct_node_t* node = NULL;
  node = csprof_state_insert_backtrace(state, metric_id, 
				       bt_end, bt_beg, sample_count);

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


// returns the end of the chord (exclusive)
static csprof_frame_t* 
canonicalize_chord(csprof_frame_t* chord_beg, lush_assoc_t as,
		   unsigned int pchord_len, unsigned int lchord_len)
{
  // Set assoc and fill empty p-notes/l-notes

  // INVARIANT: chord_len >= 1
  //   [chord_beg = innermost ... outermost, chord_end)

  unsigned int chord_len = MAX(pchord_len, lchord_len);
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
  
  unsigned int path_len = chord_len;
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
