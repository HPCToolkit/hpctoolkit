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

#include "lush.i"
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

// Given a lush_cursor with a valid pchord, compute bichord and
// lchord meta-information
LUSHI_DECL(lush_step_t, LUSHI_peek_bichord, (lush_cursor_t* cursor));

// Given a lush_cursor with a valid bichord, determine the next pnote
// (or lnote)
LUSHI_DECL(lush_step_t, LUSHI_step_pnote, (lush_cursor_t* cursor));
LUSHI_DECL(lush_step_t, LUSHI_step_lnote, (lush_cursor_t* cursor));

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
