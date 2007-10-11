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

#ifndef lush_lush_h
#define lush_lush_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <ucontext.h>

//*************************** User Include Files ****************************

#include "lushi.h"
#include "lush-support.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// LUSH Agents
//***************************************************************************

// ---------------------------------------------------------
// LUSH agent id
// ---------------------------------------------------------

#define lush_agentid_NULL 0
typedef int lush_agentid_t;


// ---------------------------------------------------------
// A LUSH agent
// ---------------------------------------------------------

typedef struct lush_agent lush_agent_t;

struct lush_agent {
  lush_agentid_t id;
  char* path;
  void* dlhandle;
};

// ---------------------------------------------------------
// A pool of LUSH agents
// ---------------------------------------------------------

typedef struct lush_agent_pool lush_agent_pool_t;

struct lush_agent_pool {

  lush_agent_t agent; // FIXME: one agent for now

  // for each LUSHI routine, a vector of pointers for each agent
  // (indexed by agent id)
#define POOL_DECL(FN) \
  FN ## _fn_t*  FN

  POOL_DECL(LUSHI_init);
  POOL_DECL(LUSHI_fini);
  POOL_DECL(LUSHI_strerror);
  POOL_DECL(LUSHI_reg_dlopen);
  POOL_DECL(LUSHI_ismycode);
  POOL_DECL(LUSHI_peek_bichord);
  POOL_DECL(LUSHI_step_pnote);
  POOL_DECL(LUSHI_step_lnote);

#undef POOL_DECL
};

int lush_agent__init(lush_agent_t* x, int id, const char* path, 
		     lush_agent_pool_t* pool);

int lush_agent__fini(lush_agent_t* x, lush_agent_pool_t* pool);


int lush_agent_pool__init(lush_agent_pool_t* x, const char* path);
int lush_agent_pool__fini(lush_agent_pool_t* x);


// **************************************************************************
// LUSH Unwinding Primitives
// **************************************************************************

// Initialize the unwind.  Set a flag indicating initialization.
void lush_init_unw(lush_cursor_t* cursor, 
		   lush_agent_pool_t* apool, mcontext_t *context);


// Given a lush_cursor, peek the next bichord.  Assumes that at most
// one agent is responsible for any IP address.
lush_step_t lush_peek_bichord(lush_cursor_t* cursor);


// Given a lush_cursor, unwind to the next physical chord and locate the
// physical cursor.  Use the appropriate agent or local procedures.
lush_step_t lush_step_pchord(lush_cursor_t* cursor);


// Given a lush_cursor, unwind one pnote/lonote of the pchord/lchord.
lush_step_t lush_step_pnote(lush_cursor_t* cursor);
lush_step_t lush_step_lnote(lush_cursor_t* cursor);


// Given a lush_cursor, forcefully advance to the next pnote (which
// may also be the next pchord).  On successful completion, returns
// LUSH_STEP_CONT; if the previous pnote was the last frame in the
// pchord, return LUSH_STEP_DONE; otherwise returns LUSH_STEP_ERROR.
lush_step_t lush_forcestep_pnote(lush_cursor_t* cursor);
lush_step_t lush_forcestep_lnote(lush_cursor_t* cursor);

// **************************************************************************

#endif /* lush_lushi_h */
