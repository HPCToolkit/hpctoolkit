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

//*************************** Forward Declarations **************************

//***************************************************************************
// backtrace
//***************************************************************************

int csprof_sample_callstack(csprof_state_t *state, 
			    int metric_id, size_t sample_count, 
			    void* context);

int lush_backtrace(csprof_state_t* state, 
		   int metric_id, size_t sample_count, 
		   mcontext_t* context);

#if 0
int
csprof_sample_callstack(csprof_state_t *state, int metric_id,
			size_t sample_count, void *context)
{
  int ret;
  ret = lush_backtrace(state, metric_id, sample_count, (mcontext_t*)context);
  ret = (ret == 0) ? CSPROF_OK : CSPROF_ERR;
  return ret;
}
#endif


int
lush_backtrace(csprof_state_t* state, 
	       int metric_id, size_t sample_count, 
	       mcontext_t* context)
{
#if 0 // FIXME
  unw_context_t uc;
  unw_getcontext(&uc);
#endif

  lush_cursor_t cursor;
  lush_init_unw(&cursor, state->lush_agents, context);

  // Step through bichords
  while (lush_step_bichord(&cursor) != LUSH_STEP_END_PROJ) {
    lush_agentid_t aid = lush_cursor_get_aid(&cursor);
    lush_assoc_t as = lush_cursor_get_assoc(&cursor);
    
    // Step through p-notes of p-chord
    while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
      unw_word_t ip = lush_cursor_get_ip(&cursor);

      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "IP %lx / %d", ip, as);
    }

    // Step through l-notes of l-chord
    while (lush_step_lnote(&cursor) != LUSH_STEP_END_CHORD) {
      lush_lip_t* lip = lush_cursor_get_lip(&cursor);
      
      DBGMSG_PUB(CSPROF_DBG_UNWINDING, "LIP[%d] %lx", aid, *((void**)lip));
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

  return 0;
}

