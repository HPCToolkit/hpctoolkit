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

//*************************** Forward Declarations **************************

static void handle_any_dlerror();

// **************************************************************************
// LUSH Agents
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
  CALL_DLSYM(pool, LUSHI_step_bichord, id, x->dlhandle);
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


// **************************************************************************
// LUSH Agent Pool
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
  FN_TBL_ALLOC(x, LUSHI_step_bichord, num_agents + 1);
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
  FN_TBL_FREE(x, LUSHI_step_bichord);
  FN_TBL_FREE(x, LUSHI_step_pnote);
  FN_TBL_FREE(x, LUSHI_step_lnote);

#undef FN_TBL_FREE
  
  return 0;
}


// **************************************************************************
// Unwind support
// **************************************************************************

void 
lush_init_unw(lush_cursor_t* cursor, 
	      lush_agent_pool_t* apool, mcontext_t* context)
{
#ifndef PRIM_UNWIND
#  error "FIXME: we only know about PRIMITIVE UNWINDING"
#endif
  
  memset(cursor, 0, sizeof(*cursor));

  cursor->apool = apool;
  lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_INIT);
  unw_init_f_mcontext(context, lush_cursor_get_pcursor(cursor));
}


lush_step_t
lush_step_bichord(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_NULL;

  // 1. Determine next physical chord (starting point of next bichord)
  ty = lush_step_pchord(cursor);

  if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
    return ty;
  }

  // 2. Compute bichord and logical chord meta-info
  unw_word_t ip = lush_cursor_get_ip(cursor);
  cursor->aid = lush_agentid_NULL;

  // 2a. First, see if an agent can interpret the pchord, giving an lchord
  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = 1;
  for (aid = 1; aid <= 1; ++aid) { // FIXME: first in list, etc.
    if (pool->LUSHI_ismycode[aid](ip)) {
      pool->LUSHI_step_bichord[aid](cursor);
      cursor->aid = aid;
      // FIXME: move agent to beginning of list;
      break;
    }
  }
  
  // 2b. Otherwise, the physical chord is the logical chord (identity agent)
  if (cursor->aid == lush_agentid_NULL) {
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
    while (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_PCHORD_DONE)) {
      lush_forcestep_pnote(cursor);
    }
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_CHORD_DONE);
      
    ty = lush_forcestep_pnote(cursor);
    
    if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
      // we have reached the outermost frame of the stack (or an error state)
      lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_CHORD_DONE);
    }
  }
  else {
    // the first pchord begins at the current pnote (pcursor)
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_INIT);
    ty = LUSH_STEP_CONT;
  }

  return ty;
}


lush_step_t 
lush_step_pnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_PCHORD_DONE)) {
    ty = lush_forcestep_pnote(cursor);
  }
  
  return ty;
}


lush_step_t
lush_step_lnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_LCHORD_DONE)) {
    ty = lush_forcestep_lnote(cursor);
  }

  return ty;
}


lush_step_t
lush_forcestep_pnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = cursor->aid;
  if (aid != lush_agentid_NULL) {
    ty = pool->LUSHI_step_pnote[aid](cursor);
  }
  else {
    int t = unw_step(lush_cursor_get_pcursor(cursor));
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
  
  if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_PCHORD_DONE);
  }
  return ty;
}


lush_step_t
lush_forcestep_lnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_CONT;

  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = cursor->aid;
  if (aid != lush_agentid_NULL) {
    ty = pool->LUSHI_step_lnote[aid](cursor);
  }
  else {
    int t = unw_step(lush_cursor_get_pcursor(cursor));
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

  if (ty == LUSH_STEP_DONE || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_LCHORD_DONE);
  }
  return ty;
}

//***************************************************************************
