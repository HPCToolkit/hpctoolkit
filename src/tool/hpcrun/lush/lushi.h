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
//
// File: 
//   $HeadURL$
//
// Purpose:
//   LUSH Interface: Interface for LUSH agents
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_lushi_h
#define lush_lushi_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include "lush-support-rt.h"
#include "lushi-cb.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

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
			     lush_agentid_t           aid,
			     LUSHCB_malloc_fn_t       malloc_fn,
			     LUSHCB_free_fn_t         free_fn,
			     LUSHCB_step_fn_t         step_fn,
			     LUSHCB_loadmap_find_fn_t loadmap_fn));

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
// Metrics
// --------------------------------------------------------------------------

LUSHI_DECL(bool, LUSHI_do_metric, (uint64_t incrMetricIn, bool* doMetric, bool* doMetricIdleness, uint64_t* incrMetric, double* incrMetricIdleness));


#ifdef __cplusplus
}
#endif

// **************************************************************************

#undef LUSHI_DECL

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_lushi_h */
