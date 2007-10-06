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

//************************* System Include Files ****************************

#include <stdlib.h>
#include <string.h>
#include <errno.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

// **************************************************************************
// A LUSH agent provides:
//   1. Facility for logical unwinding
//   2. Facility for maintaining active marker
//   3. Runtime concurrency information
// **************************************************************************

// --------------------------------------------------------------------------
// Types
// --------------------------------------------------------------------------

enum LUSHI_ret_t {
  LUSHI_UNW_Ok,
  LUSHI_UNW_Error
};

enum LUSHI_error_t {
  //...
};


// --------------------------------------------------------------------------
// Initialization/Finalization
// --------------------------------------------------------------------------

LUSHI_ret_t LUSHI_init(argc, argv);

LUSHI_ret_t LUSHI_fini();


// --------------------------------------------------------------------------
// Maintaining Responsibility for Code/Frame-space
// --------------------------------------------------------------------------

LUSHI_ret_t LUSHI_reg_dlopen();

bool LUSHI_ismycode(VMA addr);


// --------------------------------------------------------------------------
// Logical Unwinding
// --------------------------------------------------------------------------

// Given a lush_cursor with a valid pchord, compute bichord and
// lchord meta-information
lush_step_t LUSHI_peek_bichord(lush_cursor_t& cursor);

// Given a lush_cursor with a valid bichord, determine the next pnote
// (or lnote)
lush_step_t LUSHI_step_pnote(lush_cursor_t* cursor);
lush_step_t LUSHI_step_lnote(lush_cursor_t* cursor);

LUSHI_ret_t LUSHI_set_active_frame_marker(ctxt, cb);

// --------------------------------------------------------------------------

LUSHI_ret_t LUSHI_lip_destroy(lush_lip_t* lip);
LUSHI_ret_t LUSHI_lip_eq(lush_lip_t* lip);

LUSHI_ret_t LUSHI_lip_read(...);
LUSHI_ret_t LUSHI_lip_write(...);


// --------------------------------------------------------------------------
// Concurrency
// --------------------------------------------------------------------------

int LUSHI_has_concurrency();
int LUSHI_get_concurrency();

// **************************************************************************
