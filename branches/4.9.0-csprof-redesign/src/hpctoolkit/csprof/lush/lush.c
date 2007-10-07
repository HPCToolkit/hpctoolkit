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

//*************************** Forward Declarations **************************

static void handle_any_dlerror();


// **************************************************************************
// Agents and Agent pool
// **************************************************************************

int 
lush_agent_pool__init(lush_agent_pool_t* x, const char* path)
{
  int num_agents = 1; // count the agents

  // 1. Allocate tables first
#define FN_TBL_ALLOC(BASE, FN, SZ) \
  BASE->FN = (FN ## _fn_t *) malloc(sizeof(FN ## _fn_t) * (SZ))
  
  FN_TBL_ALLOC(x, LUSHI_init, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_fini, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_strerror, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_reg_dlopen, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_ismycode, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_peek_bichord, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_step_pnote, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_step_lnote, num_agents + 1);

#undef FN_TBL_ALLOC

  // 2. Now initialize agents

  // FIXME: assumes only one agent at the moment
  int aid = 1;
  const char* apath = path;
  lush_agent__init(&x->agent, aid, apath, x);

  return 0;
}


int 
lush_agent_pool__fini(lush_agent_pool_t* x)
{
#define FN_TBL_FREE(BASE, FN) \
  free(BASE->FN)
  
  FN_TBL_FREE(x, LUSHI_init);
  FN_TBL_FREE(x, LUSHI_fini);
  FN_TBL_FREE(x, LUSHI_strerror);
  FN_TBL_FREE(x, LUSHI_reg_dlopen);
  FN_TBL_FREE(x, LUSHI_ismycode);
  FN_TBL_FREE(x, LUSHI_peek_bichord);
  FN_TBL_FREE(x, LUSHI_step_pnote);
  FN_TBL_FREE(x, LUSHI_step_lnote);

#undef FN_TBL_FREE
  
  return 0;
}


// **************************************************************************

int 
lush_agent__init(lush_agent_t* x, int id, const char* path, 
		 lush_agent_pool_t* pool)
{
  x->id = id;
  x->path = strdup(path); // NOTE: assume it's safe to use malloc

  x->dlhandle = dlopen(path, RTLD_LAZY);
  handle_any_dlerror();

#define CALL_DLSYM(BASE, X, ID, HANDLE)	       \
  BASE->X[ID] = (X ## _fn_t)dlsym(HANDLE, #X); \
  handle_any_dlerror()
  
  CALL_DLSYM(pool, LUSHI_init,         id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_fini,         id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_strerror,     id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_reg_dlopen,   id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_ismycode,     id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_peek_bichord, id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_step_pnote,   id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_step_lnote,   id, x->dlhandle);

#undef CALL_DLSYM

  pool->LUSHI_init[x->id](0, NULL, 
			  (LUSHCB_malloc_fn_t)NULL, 
			  (LUSHCB_free_fn_t)NULL,
			  (LUSHCB_step_fn_t)unw_step,
			  LUSHCB_get_loadmap);
  return 0;
}


int
lush_agent__fini(lush_agent_t* x, lush_agent_pool_t* pool)
{
  pool->LUSHI_fini[x->id]();

  dlclose(x->dlhandle);
  handle_any_dlerror();

  free(x->path);
  return 0;
}


// **************************************************************************

// FIXME: Copied form dlpapi.c.  When hpcrun and csprof are merged,
// this can be merged into a common lib.
static void
handle_any_dlerror()
{
  // Note: We assume dlsym() or something similar has just been called!
  char *error;
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error); 
    exit(1);
  }
}


//***************************************************************************
// backtrace
//***************************************************************************

#if 0
void
lush_backtrace() 
{
  unw_context_t uc;
  lush_cursor_t cursor;

  unw_getcontext(&uc);
  lush_init_unw(&cursor, &uc); // sets an init flag

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
#endif


// **************************************************************************
// Unwind support
// **************************************************************************

#if 0
lush_step_t 
lush_peek_bichord(lush_cursor_t* cursor)
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
#endif
