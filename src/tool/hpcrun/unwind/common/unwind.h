// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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

#include <unwind/common/unw-datatypes.h> 
#include <hpcrun/utilities/ip-normalized.h>

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
