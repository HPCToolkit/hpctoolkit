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

void
lush_backtrace(csprof_state_t *state, 
	       int metric_id, size_t sample_count, 
	       mcontext_t* context)
{
  //lush_agent_pool_t* pool = state->lush_agents;

#if 0 // FIXME
  unw_context_t uc;
  unw_getcontext(&uc);
#endif

  lush_cursor_t cursor;
  lush_init_unw(&cursor, context);

  while (lush_peek_bichord(&cursor) != LUSH_STEP_DONE) {

    lush_assoc_t as = lush_cursor_get_assoc(&cursor);
    switch (as) {
      
      case LUSH_ASSOC_1_to_1:
	lush_step_pnote(&cursor);
	lush_step_lnote(&cursor);
	break;

      // many-to-1
      case LUSH_ASSOC_1_n_to_0:
      case LUSH_ASSOC_2_n_to_1:
	while (lush_step_pnote(&cursor) != LUSH_STEP_DONE) {
	  // ... get IP
	}
	if (as == LUSH_ASSOC_2_n_to_1) {
	  lush_step_lnote(&cursor);
	}
	break;
	
      // 1-to-many
      case LUSH_ASSOC_1_to_2_n:
      case LUSH_ASSOC_0_to_2_n:
	if (as == LUSH_ASSOC_1_to_2_n) {
	  lush_step_pnote(&cursor);
	  // ... get IP
	}

	while (lush_step_lnote(&cursor) != LUSH_STEP_DONE) {
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
}


// **************************************************************************
// Unwind support
// **************************************************************************

void 
lush_init_unw(lush_cursor_t* cursor, mcontext_t *context)
{
#ifndef PRIM_UNWIND
#  error "FIXME: we only know about PRIMITIVE UNWINDING"
#endif
  
  memset(cursor, 0, sizeof(*cursor));

  unw_init_f_mcontext(context, &cursor->pcursor);
  lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_INIT);
}


lush_step_t
lush_peek_bichord(lush_cursor_t* cursor)
{
  csprof_state_t* state = csprof_get_state(); // FIXME: abstract?
  lush_agent_pool_t* pool = state->lush_agents;

  lush_step_t ty = LUSH_STEP_NULL;

  // 1. Determine next physical chord (starting point of next bichord)
  ty = lush_step_pchord(cursor);

  if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
    return ty;
  }

  // 2. Compute bichord and logical chord meta-info
  unw_word_t ip = lush_cursor_get_ip(cursor);
  cursor->aid = lush_agentid_NULL;

  lush_agentid_t aid = 1;
  for (aid = 1; aid <= 1; ++aid) { // FIXME: first in list, etc.
    if (pool->LUSHI_ismycode[aid](ip)) {
      pool->LUSHI_peek_bichord[aid](cursor);
      cursor->aid = aid;
      // FIXME: move agent to beginning of list;
      break;
    }
  }
  
  if (cursor->aid == lush_agentid_NULL) {
    // identity function: the physical cursor is the logical cursor
    cursor->assoc = LUSH_ASSOC_1_to_1;
  }

  return ty;
}


lush_step_t
lush_step_pchord(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_NULL;

  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_INIT)) {
    // complete the current pchord, if we haven't examined all pnotes
    while (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_DONEP)) {
      lush_forcestep_pnote(cursor);
    }
    
    ty = lush_forcestep_pnote(cursor);
  }
  else {
    // the first pchord begins at the current pnote (pcursor)
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_INIT);
    ty = LUSH_STEP_CONT;
  }

  lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_INITALL);
  lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_DONEALL);
  return ty;
}


lush_step_t 
lush_step_pnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_DONEP)) {
    ty = lush_forcestep_pnote(cursor);
    if (ty == LUSH_STEP_DONE) {
      lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_DONEP);
    }
  }
  
  return ty;
}


lush_step_t
lush_step_lnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_DONEL)) {
    ty = lush_forcestep_lnote(cursor);
    if (ty == LUSH_STEP_DONE) {
      lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_DONEL);
    }
  }

  return ty;
}


lush_step_t
lush_forcestep_pnote(lush_cursor_t* cursor)
{
  csprof_state_t* state = csprof_get_state(); // FIXME: abstract?
  lush_agent_pool_t* pool = state->lush_agents;

  lush_step_t ty = LUSH_STEP_CONT;

  lush_agentid_t aid = cursor->aid;
  if (aid != lush_agentid_NULL) {
    ty = pool->LUSHI_step_pnote[aid](cursor);
  }
  else {
    int t = unw_step(&cursor->pcursor);
    if (t > 0) {
      // LUSH_STEP_CONT
    }
    else if (t == 0) {
      ty = LUSH_STEP_DONE;
    } 
    else if (t < 0) {
      ty = LUSH_STEP_ERROR;
    }
  }

  return ty;
}


lush_step_t
lush_forcestep_lnote(lush_cursor_t* cursor)
{
  return LUSH_STEP_ERROR; // FIXME
}


//***************************************************************************
// LUSH Unwind Types
//***************************************************************************

bool 
lush_cursor_is_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{ 
  return (cursor->flags & f); 
}


void 
lush_cursor_set_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{
  cursor->flags = cursor->flags | f;
}


void 
lush_cursor_unset_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{
  cursor->flags = cursor->flags & ~f;
}


lush_assoc_t 
lush_cursor_get_assoc(lush_cursor_t* cursor)
{
  return cursor->assoc;
}


unw_word_t
lush_cursor_get_ip(lush_cursor_t* cursor)
{
  unw_word_t ip = 0;
  if (unw_get_reg(&(cursor->pcursor), UNW_REG_IP, &ip) < 0) {
    // FIXME
  }
  return ip;
}
