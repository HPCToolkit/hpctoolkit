// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH Interface: Interface for LUSH agents
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_lushi_h
#define lush_lushi_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include "lush-support.h"
#include "lushi-cb.h"

//*************************** Forward Declarations **************************

// **************************************************************************
// A LUSH agent provides:
//   1. Facility for logical unwinding
//   2. Facility for maintaining active marker
//   3. Runtime concurrency information
// **************************************************************************

#ifdef __cplusplus
extern "C" {
#endif


// --------------------------------------------------------------------------
// Initialization/Finalization
// --------------------------------------------------------------------------

LUSHI_DECL(int, LUSHI_init, (int argc, char** argv,
			     LUSH_AGENTID_XXX_t      aid,
			     LUSHCB_malloc_fn_t      malloc_fn,
			     LUSHCB_free_fn_t        free_fn,
			     LUSHCB_step_fn_t        step_fn,
			     LUSHCB_get_loadmap_fn_t loadmap_fn));

LUSHI_DECL(int, LUSHI_fini, ());

LUSHI_DECL(char*, LUSHI_strerror, (int code));


// --------------------------------------------------------------------------
// Maintaining Responsibility for Code/Frame-space
// --------------------------------------------------------------------------

LUSHI_DECL(int, LUSHI_reg_dlopen, ());

LUSHI_DECL(bool, LUSHI_ismycode, (void* addr));


// --------------------------------------------------------------------------
// Logical Unwinding
// --------------------------------------------------------------------------

// Given a lush_cursor, step the cursor to the next (less deeply
// nested) bichord.  Returns:
//   LUSH_STEP_CONT:     if step was sucessful
//   LUSH_STEP_ERROR:    on account of an error.
//
// It is assumed that:
// - the cursor is initialized with the first p-note of what will be
//   the current p-chord (IOW, p-note is always valid and part of the
//   p-projection)
// - consequently, LUSH_STEP_END_PROJ is not a valid return value.
// - the predicate LUSHI_ismycode(ip) holds, where ip is the physical
//   IP from the p-chord
// - the cursor's agent-id field points to the agent responsible for
//   the last bichord (or NULL).
LUSHI_DECL(lush_step_t, LUSHI_step_bichord, (lush_cursor_t* cursor));


// Given a lush_cursor, _forcefully_ step the cursor to the next (less
// deeply nested) p-note which may also be the next p-chord.
// Returns:
//   LUSH_STEP_CONT:      if step was sucessful
//   LUSH_STEP_END_CHORD: if prev p-note was the end of the p-chord
//   LUSH_STEP_END_PROJ:  if prev p-chord was end of p-projection
//   LUSH_STEP_ERROR:     on account of an error.
LUSHI_DECL(lush_step_t, LUSHI_step_pnote, (lush_cursor_t* cursor));


// Given a lush_cursor, step the cursor to the next (less deeply
// nested) l-note of the current l-chord.
// Returns: 
//   LUSH_STEP_CONT:      if step was sucessful (only possible if not a-to-0)
//   LUSH_STEP_END_CHORD: if prev l-note was the end of the l-chord
//   LUSH_STEP_ERROR:     on account of an error.
LUSHI_DECL(lush_step_t, LUSHI_step_lnote, (lush_cursor_t* cursor));


// ...
LUSHI_DECL(int, LUSHI_set_active_frame_marker, (/*context, callback*/));

// --------------------------------------------------------------------------

LUSHI_DECL(int, LUSHI_lip_destroy, (lush_lip_t* lip));
LUSHI_DECL(int, LUSHI_lip_eq, (lush_lip_t* lip));

LUSHI_DECL(int, LUSHI_lip_read, ());
LUSHI_DECL(int, LUSHI_lip_write, ());


// --------------------------------------------------------------------------
// Concurrency
// --------------------------------------------------------------------------

LUSHI_DECL(int, LUSHI_has_concurrency, ());
LUSHI_DECL(int, LUSHI_get_concurrency, ());


#ifdef __cplusplus
}
#endif

// **************************************************************************

#undef LUSHI_DECL

#endif /* lush_lushi_h */
