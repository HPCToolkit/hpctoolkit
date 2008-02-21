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
#if 0 // FIXME
  unw_context_t uc;
  unw_getcontext(&uc);
#endif

  if (getenv("LUSH_WAIT")) {
    DBGMSG_PUB(1, "LUSH_WAIT... (pid=%d)", getpid());
    while(LUSH_WAIT);
  }

  lush_cursor_t cursor;
  lush_init_unw(&cursor, state->lush_agents, context);

  // Step through bichords
  while (lush_step_bichord(&cursor) != LUSH_STEP_END_PROJ) {
    lush_agentid_t aid = lush_cursor_get_aid(&cursor);
    lush_assoc_t as = lush_cursor_get_assoc(&cursor);

    DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Chord: aid:%d assoc:%d", aid, as);
  
    // Step through p-notes of p-chord
    while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
      unw_word_t ip = lush_cursor_get_ip(&cursor);

      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "IP:  %p", ip);
    }

    // Step through l-notes of l-chord
    while (lush_step_lnote(&cursor) != LUSH_STEP_END_CHORD) {
      lush_lip_t* lip = lush_cursor_get_lip(&cursor);
      
      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "LIP: %p", *((void**)lip));
    }
    
    // FIXME: insert backtrace into spine-tree and bump sample counter
  }

  // FIXME: register active marker

  // look at concurrency for agent at top of stack
#if 0 // FIXME
  if (LUSHI_has_concurrency[agent]() ...) {
    lush_agentid_t aid = 1;
    pool->LUSHI_get_concurrency[aid]();
  }
#endif

  return (csprof_cct_node_t*)(1); // FIXME: bogus non-NULL value
}

