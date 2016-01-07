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
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_lush_h
#define lush_lush_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <ucontext.h>

//*************************** User Include Files ****************************

#include "lushi.h"
#include "lush-support-rt.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// LUSH Agents
//***************************************************************************

// ---------------------------------------------------------
// A pool of LUSH agents
// ---------------------------------------------------------

// FIXME: put this and agent decls in lush-support.h into one file.
struct lush_agent_pool {

  lush_agent_t agent; // FIXME: one agent for now

  int metric_time;     // FIXME: only one for now
  int metric_idleness; // idleness is relative to time

  // for each LUSHI routine, a vector of pointers for each agent
  // (indexed by agent id)
#define POOL_DECL(FN) \
  FN ## _fn_t*  FN

  POOL_DECL(LUSHI_init);
  POOL_DECL(LUSHI_fini);
  POOL_DECL(LUSHI_strerror);
  POOL_DECL(LUSHI_reg_dlopen);
  POOL_DECL(LUSHI_ismycode);
  POOL_DECL(LUSHI_step_bichord);
  POOL_DECL(LUSHI_step_pnote);
  POOL_DECL(LUSHI_step_lnote);
  POOL_DECL(LUSHI_do_metric);

#undef POOL_DECL
};

int
lush_agent__init(lush_agent_t* x, int id, const char* path, 
		 lush_agent_pool_t* pool);

int
lush_agent__fini(lush_agent_t* x, lush_agent_pool_t* pool);


int
lush_agent_pool__init(lush_agent_pool_t* x, const char* path);

int
lush_agent_pool__fini(lush_agent_pool_t* x);


//***************************************************************************
// LUSH Unwinding Interface
//***************************************************************************

// Given an agent-pool and context, initialize the lush_cursor but do
// not step to the first (innermost) bichord.
void
lush_init_unw(lush_cursor_t* cursor, 
	      lush_agent_pool_t* apool, ucontext_t* context);


// Given a lush_cursor, step the cursor to the next (less deeply
// nested) bichord.  Returns:
//   LUSH_STEP_CONT:     if step was sucessful
//   LUSH_STEP_END_PROJ: if chord was end of projection
//   LUSH_STEP_ERROR:    on account of an error.
lush_step_t
lush_step_bichord(lush_cursor_t* cursor);


// Given a lush_cursor, step the cursor to the next (less deeply
// nested) p-note/l-note of the current p-chord/l-chord.
// Returns: 
//   LUSH_STEP_CONT:      if step was sucessful
//                        (for l-notes, only possible if not a-to-0)
//   LUSH_STEP_END_CHORD: if prev note was the end of the chord
//   LUSH_STEP_ERROR:     on account of an error.
lush_step_t
lush_step_pnote(lush_cursor_t* cursor);

lush_step_t
lush_step_lnote(lush_cursor_t* cursor);


//***************************************************************************
// LUSH Unwinding Primitives
//***************************************************************************

// Given a lush_cursor, _forcefully_ step the cursor to the next (less
// deeply nested) p-chord.  Return values are same as
// lush_step_bichord.
lush_step_t
lush_forcestep_pchord(lush_cursor_t* cursor);


// Given a lush_cursor, _forcefully_ step the cursor to the next (less
// deeply nested) p-note which may also be the next p-chord.
// Returns:
//   LUSH_STEP_CONT:      if step was sucessful
//   LUSH_STEP_END_CHORD: if prev p-note was the end of the p-chord
//   LUSH_STEP_END_PROJ:  if prev p-chord was end of p-projection
//   LUSH_STEP_ERROR:     on account of an error.
//
// Sets zero or more of the following flags (as appropriate):
//   LUSH_CURSOR_FLAGS_END_PPROJ:  
//   LUSH_CURSOR_FLAGS_BEG_PCHORD: 
//   LUSH_CURSOR_FLAGS_END_PCHORD: 
lush_step_t
lush_forcestep_pnote(lush_cursor_t* cursor);


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_lush_h */
