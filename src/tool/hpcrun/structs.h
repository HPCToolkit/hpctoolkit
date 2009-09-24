// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

/* out-of-line defintions for library structures */
#ifndef CSPROF_STRUCTS_H
#define CSPROF_STRUCTS_H

#include <limits.h>             /* PATH_MAX */
#include <sys/types.h>          /* pid_t, e.g. */

#include "cct.h"
#include "epoch.h"

#include <lush/lush.h>

// FIXME: tallent: Collecting all these defs here is a VERY BAD IDEA
// because it easily results in circular header dependencies.  They
// should be distributed.  I have already distributed csprof_state
// stuff.

#define CSPROF_PATH_SZ (PATH_MAX+1) /* path size */

// ---------------------------------------------------------
// profiling state of a single thread
// ---------------------------------------------------------

typedef struct csprof_state_t {

  /* information for recording function call returns; do not move this
     block, since `swizzle_return' must be the first member of the
     structure if we are doing backtracing */

  void *swizzle_return;
  void **swizzle_patch;

  /* last pc where we were signaled; useful for catching problems in
     threaded builds of the profiler (can be useful for debugging in
     non-threaded builds, too) */

  void *last_pc;
  void *unwind_pc;
  hpcrun_frame_t *unwind;
  void *context_pc;

  // btbuf                                                      bufend
  // |                                                            |
  // v low VMAs                                                   v
  // +------------------------------------------------------------+
  // [new backtrace         )              [cached backtrace      )
  // +------------------------------------------------------------+
  //                        ^              ^ 
  //                        |              |
  //                      unwind         bufstk
  

  hpcrun_frame_t *btbuf;      // innermost frame in new backtrace
  hpcrun_frame_t *bufend;     // 
  hpcrun_frame_t *bufstk;     // innermost frame in cached backtrace
  csprof_cct_node_t*   treenode;   // cached pointer into the tree

  /* how many bogus samples we took */
  unsigned long trampoline_samples;

  /* various flags, such as whether an exception is being processed or
     whether we think there was a tail call since the last signal */
  unsigned int flags;

  /* call stack data, stored in private memory */
  hpcrun_cct_t csdata;
  lush_cct_ctxt_t* csdata_ctxt; // creation context

  /* our notion of what the current epoch is */
  hpcrun_epoch_t *epoch;

  /* other profiling states which we have seen */
  struct csprof_state_t* next;

  /* support for alternate profilers whose needs we don't provide */
  void *extra_state;

} csprof_state_t;

#define CSPROF_SAMPLING_ENABLED 1

#define CLEAR_FLAGS(state, flagvals) state->flags &= ~(flagvals)
#define SET_FLAGS(state, flagvals) state->flags |= (flagvals)
#define TEST_FLAGS(state, flagvals) (state->flags & (flagvals))

#endif
