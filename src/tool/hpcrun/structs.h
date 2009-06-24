// -*-Mode: C++;-*- // technically C99
// $Id$

/* out-of-line defintions for library structures */
#ifndef CSPROF_STRUCTS_H
#define CSPROF_STRUCTS_H

#include <limits.h>             /* PATH_MAX */
#include <sys/types.h>          /* pid_t, e.g. */

#include "csprof_csdata.h"
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

typedef struct csprof_state_s {

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
  csprof_frame_t *unwind;
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
  

  csprof_frame_t *btbuf;      // innermost frame in new backtrace
  csprof_frame_t *bufend;     // 
  csprof_frame_t *bufstk;     // innermost frame in cached backtrace
  void *treenode;             /* cached pointer into the tree */

  /* how many bogus samples we took */
  unsigned long trampoline_samples;

  /* various flags, such as whether an exception is being processed or
     whether we think there was a tail call since the last signal */
  unsigned int flags;

  /* call stack data, stored in private memory */
  csprof_csdata_t csdata;
  lush_cct_ctxt_t* csdata_ctxt; // creation context

  /* our notion of what the current epoch is */
  csprof_epoch_t *epoch;

  /* other profiling states which we have seen */
  struct csprof_state_s *next;

  /* support for alternate profilers whose needs we don't provide */
  void *extra_state;

} csprof_state_t;

#define CSPROF_SAMPLING_ENABLED 1

#define CLEAR_FLAGS(state, flagvals) state->flags &= ~(flagvals)
#define SET_FLAGS(state, flagvals) state->flags |= (flagvals)
#define TEST_FLAGS(state, flagvals) (state->flags & (flagvals))

#endif
