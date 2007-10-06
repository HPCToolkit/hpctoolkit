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

//*************************** User Include Files ****************************

#include "lush.h"

//*************************** Forward Declarations **************************

//***************************************************************************
//
//***************************************************************************


// ---------------------------------------------------------
// Initialization summary:
// ---------------------------------------------------------
//   dlopen each agent and dlsym TUNA routines
//   construct an id<->agent map
//   
//   foreach agent (a, agent-id)
//     (LUSHI_init[agent-id])(...);


// ---------------------------------------------------------
// Sampling the logical stack
// ---------------------------------------------------------
//   compute the physical-logical backtrace, stopping if we see 
//     the active marker
//   set the active marker
//   add backtrace to Log-CCT


// ---------------------------------------------------------
// Finalization summary:
// ---------------------------------------------------------
//   LUSHI_fini(...);
//   write id<->agent map


// ---------------------------------------------------------
// backtrace
// ---------------------------------------------------------
void
lush_backtrace() 
{
  unw_context_t uc;
  lush_cursor_t cursor;

  unw_getcontext(&uc);
  lush_init(&cursor, &uc); // sets an init flag

  while (lush_peek_bichord(&cursor) != LUSH_STEP_DONE) {

    lush_assoc_t as = lush_cursor_get_assoc(cursor);
    switch (as) {
      
      case LUSH_ASSOC_1_to_1: {
	lush_step_pnote(cursor);
	lush_step_lnote(cursor);
      }

	// many-to-1
      case LUSH_ASSOC_1_n_to_0:
      case LUSH_ASSOC_2_n_to_1: {
	while (lush_step_pnote(cursor) != LUSH_STEP_DONE) {
	  // ... get IP
	}
	if (as == LUSH_ASSOC_2_n_to_1) {
	  lush_step_lnote(cursor);
	}
      }
	
	// 1-to-many
      case LUSH_ASSOC_1_to_2_n:
      case LUSH_ASSOC_0_to_2_n: {
	if (as = LUSH_ASSOC_1_to_2_n) {
	  lush_step_pnote(cursor);
	  // ... get IP
	}

	while (lush_step_lnote(cursor) != LUSH_STEP_DONE) {
	  lush_agentid_t aid = lush_cursor_get_agent(cursor);
	  lush_lip_t* lip = lush_cursor_get_lip(cursor);
	  // ...
	}
      }

    }

    // insert backtrace into spine-tree and bump sample counter
  }

  // register active marker [FIXME]

  // look at concurrency for agent at top of stack
  if (... LUSHI_has_concurrency[agent]() ...) {
    LUSHI_get_concurrency[agent]();
  }
}


// **************************************************************************
// 
// **************************************************************************

lush_step_t 
lush_peek_bichord(lush_cursor_t& cursor)
{
  lush_step_t ty = LUSH_STEP_NULL;

  // 1. Determine next physical chord (starting point of next bichord)
  ty = lush_step_pchord(cursor);

  if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
    return ty;
  }

  // 2. Compute bichord and logical chord meta-info

  // zero cursor's logical agent pointer;
  foreach (agent, starting with first in list) {
    if (LUSHI_ismycode[agent](the-ip)) {
      LUSHI_peek_bichord[agent](cursor);
      // set cursor's logical agent pointer;
      // move agent to beginning of list;
      break;
    }
  }
  
  if (cursor-has-no-agent) { 
    // handle locally
    set logical flags;
  }
}


lush_step_t
lush_step_pchord(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_NULL;

  if (cursor is fully initialized) {
    if (current pchord is outstanding) {
      complete it;
    }

    ty = lush_forcestep_pnote(cursor);
  }
  else {
    physical cursor is the current mcontext;
    set cursor_is_initialized bit;
    ty = LUSH_STEP_CONT;
  }

  return ty;
}


lush_step_t 
lush_step_pnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!bichord_pnote_flag_init) {
    ty = lush_forcestep_pnote(cursor);
    set bichord_pnote_flag_init;
  }
  
  return ty;
}


lush_step_t
lush_step_lnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!bichord_lnote_flag_init) {
    ty = lush_forcestep_lnote(cursor);
    set bichord_lnote_flag_init;
  }

  return ty;
}


lush_step_t
lush_forcestep_pnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;
  
  agent = get logical agent from cursor;
  if (agent) {
    ty = LUSHI_substep_phys[agent](cursor);
  }
  else {
    t = unw_step(cursor->phys_cursor);
    ty = convert_to_ty(t);
  }

  return ty;
}

