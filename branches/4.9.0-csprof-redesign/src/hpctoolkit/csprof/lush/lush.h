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

//*************************** User Include Files ****************************

#include "lush.i"
#include "lushi.h"

//*************************** Forward Declarations **************************


//***************************************************************************
// LUSH Agents
//***************************************************************************

// NOTE: This is here instead of <lush.i> because it depends on
// <lushi.h> types.
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


//***************************************************************************
// LUSH Unwind Types
//***************************************************************************

inline lush_assoc_t 
lush_cursor_get_assoc(lush_cursor_t* cursor);

inline lush_lip_t 
lush_cursor_get_lip(lush_cursor_t* cursor);

inline void* 
lush_cursor_get_pcursor(lush_cursor_t* cursor);

inline void* 
lush_cursor_get_lcursor(lush_cursor_t* cursor);


// **************************************************************************
// LUSH Unwinding
// **************************************************************************

// Initialize the unwind.  Set a flag indicating initialization.
void lush_init_unw(lush_cursor_t* cursor, void* context);;


// Given a lush_cursor, peek the next bichord.
lush_step_t lush_peek_bichord(lush_cursor_t* cursor);


// Given a lush_cursor, unwind to the next physical chord and locate the
// physical cursor.  Use the appropriate agent or local procedures.
lush_step_t lush_step_pchord(lush_cursor_t* cursor);


// Given a lush_cursor, unwind one pnote/lonote of the pchord/lchord.
lush_step_t lush_step_pnote(lush_cursor_t* cursor);
lush_step_t lush_step_lnote(lush_cursor_t* cursor);


// Given a lush_cursor, forcefully advance to the next pnote (which
// may also be the next pchord)
lush_step_t lush_forcestep_pnote(lush_cursor_t* cursor);

// **************************************************************************

#endif /* lush_lushi_h */
