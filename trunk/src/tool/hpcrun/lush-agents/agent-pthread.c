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
//    ...
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

#include <string.h>

#include <assert.h>

//*************************** User Include Files ****************************

#include "agent-pthread.h"

#include "thread_data.h" // FIXME: outside of interface

//*************************** Forward Declarations **************************

#define LUSHCB_DECL(FN) \
 LUSH ## FN ## _fn_t  FN

LUSHCB_DECL(CB_malloc);
LUSHCB_DECL(CB_free);
LUSHCB_DECL(CB_step);
LUSHCB_DECL(CB_loadmap_find);
// lush_cursor stuff

#undef LUSHCB_DECL

static lush_agentid_t MY_lush_aid;

// **************************************************************************
// Initialization/Finalization
// **************************************************************************

extern int
LUSHI_init(int argc, char** argv,
	   lush_agentid_t           aid,
	   LUSHCB_malloc_fn_t       malloc_fn,
	   LUSHCB_free_fn_t         free_fn,
	   LUSHCB_step_fn_t         step_fn,
	   LUSHCB_loadmap_find_fn_t loadmap_fn)
{
  MY_lush_aid = aid;

  CB_malloc       = malloc_fn;
  CB_free         = free_fn;
  CB_step         = step_fn;
  CB_loadmap_find = loadmap_fn;

  return 0;
}


extern int 
LUSHI_fini()
{
  return 0;
}


extern char* 
LUSHI_strerror(int code)
{
  return ""; // STUB
}


// **************************************************************************
// Maintaining Responsibility for Code/Frame-space
// **************************************************************************

extern int 
LUSHI_reg_dlopen()
{
  return 0; // FIXME: coordinate with dylib stuff
}


extern bool 
LUSHI_ismycode(void* addr)
{
  // NOTE: Currently, this does not prevent our LUSHI_do_backtrace
  // from being called, but it may not be quite right in the context
  // of multiple agents.
  return false; // force LUSH to use the identity logical unwind
}


// **************************************************************************
// 
// **************************************************************************

extern lush_step_t
LUSHI_step_bichord(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_bichord: should never be called");
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_pnote: should never be called");
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_lnote: should never be called");
  return LUSH_STEP_ERROR;
}


extern int 
LUSHI_set_active_frame_marker(/*ctxt, cb*/)
{
  return 0; // STUB
}


// **************************************************************************
// 
// **************************************************************************

extern int
LUSHI_lip_destroy(lush_lip_t* lip)
{
  return 0; // STUB
}


extern int 
LUSHI_lip_eq(lush_lip_t* lip)
{
  return 0; // STUB
}


extern int
LUSHI_lip_read()
{
  return 0; // STUB
}


extern int
LUSHI_lip_write()
{
  return 0; // STUB
}


// **************************************************************************
// Concurrency
// **************************************************************************

extern int
LUSHI_do_backtrace()
{
  lush_pthr_t* pthr = &TD_GET(pthr_metrics);
  return (int)pthr->is_working;
}


extern double
LUSHI_get_idleness()
{
  lush_pthr_t* pthr = &TD_GET(pthr_metrics);
  if (!pthr->is_working) {
    return 0.0;
  }

  double num_working      = *(pthr->ps_num_working);
  double num_working_lock = *(pthr->ps_num_working_lock);
  double num_idle_cond    = *(pthr->ps_num_idle_cond);

  if (lush_pthr__isWorking_lock(pthr)) {
    // is_working_lock() : num_idle_lock / num_working_lock
    double idleness = (*(pthr->ps_num_threads) - num_working);
    double num_idle_lock = (idleness - num_idle_cond);
    return (num_idle_lock / num_working_lock);
  }
  else {
    // is_working_cond() || is_working : num_idle_cond / num_working_othr
    double num_working_othr = (num_working - num_working_lock);
    return (num_idle_cond / num_working_othr);
  }
}

// **************************************************************************
