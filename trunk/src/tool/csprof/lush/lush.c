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

//************************** Xtrnl Include Files ****************************

#include <monitor.h>

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

  //x->dlhandle = dlopen(path, RTLD_LAZY);
  x->dlhandle = monitor_real_dlopen(path, RTLD_LAZY);
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
  CALL_DLSYM(pool, LUSHI_has_idleness, id, x->dlhandle);
  CALL_DLSYM(pool, LUSHI_get_idleness, id, x->dlhandle);

#undef CALL_DLSYM

  pool->LUSHI_init[x->id](0, NULL, id,
			  (LUSHCB_malloc_fn_t)NULL, 
			  (LUSHCB_free_fn_t)NULL,
			  (LUSHCB_step_fn_t)unw_step,
			  LUSHCB_loadmap_find);
  return 0;
}


int
lush_agent__fini(lush_agent_t* x, lush_agent_pool_t* pool)
{
  pool->LUSHI_fini[x->id]();

  //dlclose(x->dlhandle);
  monitor_real_dlclose(x->dlhandle);
  handle_any_dlerror();

  free(x->path);
  return 0;
}

// **************************************************************************

// FIXME: Copied from dlpapi.c.  When hpcrun and csprof are merged,
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

  x->metric_id = 1; // FIXME: (ITIMER_METRIC_ID + 1)

  // 1. Allocate tables first
#define FN_TBL_ALLOC(BASE, FN, SZ) \
  BASE->FN = (FN ## _fn_t *) malloc(sizeof(FN ## _fn_t) * (SZ))
  
  FN_TBL_ALLOC(x, LUSHI_init,            num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_fini,            num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_strerror,        num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_reg_dlopen,      num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_ismycode,        num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_step_bichord,    num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_step_pnote,      num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_step_lnote,      num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_has_idleness, num_agents + 1);
  FN_TBL_ALLOC(x, LUSHI_get_idleness,    num_agents + 1);

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
  FN_TBL_FREE(x, LUSHI_has_idleness);
  FN_TBL_FREE(x, LUSHI_get_idleness);

#undef FN_TBL_FREE
  
  return 0;
}


// **************************************************************************
// LUSH Unwinding Interface
// **************************************************************************

void 
lush_init_unw(lush_cursor_t* cursor, 
	      lush_agent_pool_t* apool, ucontext_t* context)
{
  memset(cursor, 0, sizeof(*cursor));
  
  cursor->apool = apool;
  lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ);
  lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PCHORD);
  unw_init_cursor(context, lush_cursor_get_pcursor(cursor));
}


lush_step_t
lush_step_bichord(lush_cursor_t* cursor)
{
  // INVARIANT: the cursor is initialized with the first p-note of
  // what will be the current p-chord (LUSH_CURSOR_FLAGS_BEG_PCHORD),
  // or has the LUSH_CURSOR_FLAGS_END_PPROJ flag set.

  lush_step_t ty = LUSH_STEP_NULL;

  // 1. Sanity check
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_END_PPROJ)) {
    return LUSH_STEP_END_PROJ;
  }

  // 2. Officially step to next bichord.  First attempt to use an
  // agent to interpret the p-chord.  Otherwise, use the 'identity
  // agent'
  lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_END_PCHORD);
  lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_END_LCHORD);

  unw_word_t ip = lush_cursor_get_ip(cursor);
  lush_agentid_t first_aid = lush_agentid_NULL;

  // attempt to find an agent
  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = 1;
  for (aid = 1; aid <= 1; ++aid) { // FIXME: first in list, etc.
    if (pool->LUSHI_ismycode[aid](ip)) {
      ty = pool->LUSHI_step_bichord[aid](cursor);
      lush_cursor_set_aid_prev(cursor, aid);
      first_aid = aid;
      // FIXME: move agent to beginning of list;
      break;
    }
  }

  // use the 'identity agent'
  if (first_aid == lush_agentid_NULL) {
    ty = LUSH_STEP_CONT;
    lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
  }

  // 3. Set the cursor's agentid
  lush_cursor_set_aid(cursor, first_aid);

  return ty;
}


lush_step_t 
lush_step_pnote(lush_cursor_t* cursor)
{
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_END_PCHORD)) {
    return LUSH_STEP_END_CHORD;
  }

  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ)) {
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ);
  }

  lush_step_t ty;

  // Special case: LUSH_CURSOR_FLAGS_BEG_PCHORD is set
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PCHORD)) {
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PCHORD);
    if (lush_cursor_get_assoc(cursor) == LUSH_ASSOC_0_to_0) {
      // Special case: LUSH_ASSOC_0_to_0
      ty = LUSH_STEP_END_CHORD;
    }
    else {
      return LUSH_STEP_CONT;
    }
  }

  ty = lush_forcestep_pnote(cursor);
  
  // filter return type
  if (ty == LUSH_STEP_END_PROJ) {
    ty = LUSH_STEP_END_CHORD;
  }
  return ty;
}


lush_step_t
lush_step_lnote(lush_cursor_t* cursor)
{
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_END_LCHORD)) {
    return LUSH_STEP_END_CHORD;
  }

  lush_step_t ty = LUSH_STEP_NULL;

  // Step cursor to next l-note, using the appropriate agent
  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = lush_cursor_get_aid(cursor);
  if (aid != lush_agentid_NULL) {
    ty = pool->LUSHI_step_lnote[aid](cursor);
  }
  else {
    // Identity agent: Association is 1-to-1, so l-chord is unit length
    int t = unw_step(lush_cursor_get_pcursor(cursor));
    if (t > 0) {
      ty = LUSH_STEP_CONT;
      lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_END_LCHORD);
    }
    else if (t == 0) {
      ty = LUSH_STEP_END_PROJ; // must filter (invalid return type)
    } 
    else if (t < 0) {
      ty = LUSH_STEP_ERROR;
    }
  }

  // Set cursor flags
  if (ty == LUSH_STEP_END_CHORD
      || ty == LUSH_STEP_END_PROJ
      || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_END_LCHORD);
  }
  if (ty == LUSH_STEP_END_PROJ
      || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_END_LPROJ);
  }

  // filter return type
  if (ty == LUSH_STEP_END_PROJ) {
    ty = LUSH_STEP_END_CHORD;
  }

  return ty;
}


// **************************************************************************
// LUSH Unwinding Primitives
// **************************************************************************

lush_step_t
lush_step_pchord(lush_cursor_t* cursor)
{
 
  lush_step_t ty = LUSH_STEP_NULL;

#if 0 // FIXME: check this
  if (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_INIT)) {
    // complete the current p-chord, if we haven't examined all p-notes
    while (!lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_DONE_PCHORD)) {
      lush_forcestep_pnote(cursor);
    }
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_DONE_CHORD);
      
    ty = lush_forcestep_pnote(cursor);
    
    if (ty == LUSH_STEP_DONE_CHORDS || ty == LUSH_STEP_ERROR) {
      // we have reached the outermost frame of the stack (or an error state)
      lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_DONE_CHORD);
    }
  }
  else {
    // the first p-chord begins at the current p-note (p-cursor)
    lush_cursor_unset_flag(cursor, LUSH_CURSOR_FLAGS_INIT);
    ty = LUSH_STEP_CONT;
  }
#endif

  return ty;
}


lush_step_t
lush_forcestep_pnote(lush_cursor_t* cursor)
{
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_END_PPROJ)) {
    return LUSH_STEP_END_PROJ;
  }

  lush_step_t ty = LUSH_STEP_NULL;
  
  // Step cursor to next p-note, using the appropriate agent
  lush_agent_pool_t* pool = cursor->apool;
  lush_agentid_t aid = lush_cursor_get_aid(cursor);
  if (aid != lush_agentid_NULL) {
    ty = pool->LUSHI_step_pnote[aid](cursor);
  }
  else {
    // Identity agent: Association is 1-to-1, so p-chord is unit length
    int t = unw_step(lush_cursor_get_pcursor(cursor));
    if (t > 0) {
      ty = LUSH_STEP_END_CHORD;
    }
    else if (t == 0) {
      ty = LUSH_STEP_END_PROJ;
    } 
    else /* (t < 0) */ {
      ty = LUSH_STEP_ERROR;
    }
  }

  // Set cursor flags
  if (ty == LUSH_STEP_END_CHORD) {
    // since prev p-note was end of p-chord, the cursor is pointing to
    // the beginning of the next p-chord
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PCHORD);
  }
  if (ty == LUSH_STEP_END_CHORD
      || ty == LUSH_STEP_END_PROJ
      || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_END_PCHORD);
  }
  if (ty == LUSH_STEP_END_PROJ
      || ty == LUSH_STEP_ERROR) {
    lush_cursor_set_flag(cursor, LUSH_CURSOR_FLAGS_END_PPROJ);
  }
  return ty;
}


//***************************************************************************
