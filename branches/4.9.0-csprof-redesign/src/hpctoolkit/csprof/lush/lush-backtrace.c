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

int lush_backtrace(csprof_state_t* state, 
		   int metric_id, size_t sample_count, 
		   mcontext_t* context);

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

  while (lush_step_bichord(&cursor) != LUSH_STEP_END_PROJ) {

    lush_assoc_t as = lush_cursor_get_assoc(&cursor);
    switch (as) {
      
      case LUSH_ASSOC_1_to_1:
	lush_step_pnote(&cursor);
	lush_step_lnote(&cursor);
	break;

      case LUSH_ASSOC_1_to_0:
      case LUSH_ASSOC_2_n_to_0:
      case LUSH_ASSOC_2_n_to_1:
	while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
	  // ... get IP
	}
	if (as == LUSH_ASSOC_2_n_to_1) {
	  lush_step_lnote(&cursor);
	}
	break;
	
      case LUSH_ASSOC_1_to_2_n:
	lush_step_pnote(&cursor);
	// ... get IP

	while (lush_step_lnote(&cursor) != LUSH_STEP_END_CHORD) {
	  //lush_agentid_t aid = lush_cursor_get_agent(&cursor);
	  //lush_lip_t* lip = lush_cursor_get_lip(&cursor);
	  // ...
	}
	break;
	
      default:
        ERRMSG("default case reached (%d) ", __FILE__, __LINE__, as);

    }

    // insert backtrace into spine-tree and bump sample counter
  }

  // register active marker [FIXME]

  // look at concurrency for agent at top of stack
#if 0 // FIXME
  if (LUSHI_has_concurrency[agent]() ...) {
    lush_agentid_t aid = 1;
    pool->LUSHI_get_concurrency[aid]();
  }
#endif

  return 0;
}

