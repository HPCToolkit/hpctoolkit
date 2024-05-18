// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
// File:
//   unwind.h
//
// Purpose:
//   interface to the stack-unwinding primitives.
//
//***************************************************************************

#ifndef unwind_h
#define unwind_h

//***************************************************************************
// system include files
//***************************************************************************

#include <ucontext.h>


//***************************************************************************
// local include files
//***************************************************************************

#include "unw-datatypes.h"
#include "../../utilities/ip-normalized.h"

//***************************************************************************
//
// interface to HPCToolkit's unwinder (cf. libunwind)
//
//   these routines are called by the common backtrace routines and
//   therefore must be provided by architecture-specific unwind
//   agents.
//
//***************************************************************************

// ----------------------------------------------------------
// hpcrun_unw_init
// ----------------------------------------------------------

void
hpcrun_unw_init();


// ----------------------------------------------------------
// hpcrun_unw_get_ip_reg
// ----------------------------------------------------------

int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* c,
                           ip_normalized_t* reg_value);

int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* c, void** reg_value);


void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* c);

// ----------------------------------------------------------
// hpcrun_unw_init_cursor
// ----------------------------------------------------------

void
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context);


// ----------------------------------------------------------
// hpcrun_unw_step:
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame.  Conforms to the semantics of libunwind's
//   hpcrun_unw_step.  In particular, returns:
//     > 0 : successfully advanced cursor to next frame
//       0 : previous frame was the end of the unwind
//     < 0 : error condition
// ---------------------------------------------------------

typedef enum {
  STEP_ERROR = -1,
  STEP_STOP  = 0,
  STEP_OK    = 1,
  STEP_TROLL = 2,
  STEP_STOP_WEAK = 3
} step_state;


step_state
hpcrun_unw_step(hpcrun_unw_cursor_t* c);


//***************************************************************************
//
// services provided by HPCToolkit's unwinder
//
//   these routines are called by the common backtrace routines and
//   therefore must be provided by architecture-specific unwind
//   agents.
//
//***************************************************************************

// ----------------------------------------------------------
// hpcrun_unw_troll_stack
// ----------------------------------------------------------

// FIXME: tallent: move stack trolling code here


//***************************************************************************

#endif // unwind_h
