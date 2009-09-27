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

/* functions operating on thread-local state */
#ifndef CSPROF_STATE_H
#define CSPROF_STATE_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <cct/cct.h>
#include "epoch.h"

#include <lush/lush.h>

#include <messages/messages.h>

//*************************** Datatypes **************************

//***************************************************************************

// ---------------------------------------------------------
// profiling state of a single thread
// ---------------------------------------------------------

typedef struct state_t {

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
  struct state_t* next;

  /* support for alternate profilers whose needs we don't provide */
  void *extra_state;

} state_t;

//*************************** Forward Declarations **************************

//***************************************************************************

typedef state_t *state_t_f(void);

typedef void state_t_setter(state_t *s);

// ---------------------------------------------------------
// getting and setting states independent of threading support
// ---------------------------------------------------------

extern void hpcrun_reset_state(state_t* state);
extern void csprof_set_state(state_t* s);

extern int csprof_state_init(state_t* s);
extern int csprof_state_alloc(state_t *x, lush_cct_ctxt_t* thr_ctxt);

extern state_t *csprof_check_for_new_epoch(state_t *);

// ---------------------------------------------------------
// expand the internal backtrace buffer
// ---------------------------------------------------------

// tallent: move this macro here from processor/x86-64/backtrace.c.
// Undoubtedly a better solution than this is possible, but this at
// least is a more appropriate location.

#define csprof_state_ensure_buffer_avail(state, unwind)               \
  if (unwind == state->bufend) {				      \
    unwind = csprof_state_expand_buffer(state, unwind);		      \
    state->bufstk = state->bufend;				      \
  }

hpcrun_frame_t*
csprof_state_expand_buffer(state_t *, hpcrun_frame_t *);


csprof_cct_node_t* 
hpcrun_state_insert_backtrace(state_t *, int, hpcrun_frame_t *,
			      hpcrun_frame_t *, cct_metric_data_t);

#if defined(CSPROF_PERF)
#define csprof_state_verify_backtrace_invariants(state)
#else
#define csprof_state_verify_backtrace_invariants(state) \
do { \
    int condition = (state->btbuf < state->bufend) /* obvious */\
        && (state->btbuf <= state->bufstk) /* stk between beg and end */\
        && (state->bufstk <= state->bufend);\
\
    if(!condition) {\
        DIE("Backtrace invariant violated", __FILE__, __LINE__);\
    }\
} while(0);
#endif

/* finalize various parts of a state */
int csprof_state_fini(state_t *);
/* destroy dynamically allocated portions of a state */
int csprof_state_free(state_t *);

#define csprof_state_has_empty_backtrace(state) ((state)->bufend - (state)->bufstk == 0)
#define csprof_bt_pop(state) do { state->bufstk++; } while(0)
#define csprof_bt_top_ip(state) (state->bufstk->ip)
#define csprof_bt_top_sp(state) (state->bufstk->sp)
/* `ntop' == `next top'; any better ideas? */
#define csprof_bt_ntop_ip(state) ((state->bufstk + 1)->ip)
#define csprof_bt_ntop_sp(state) ((state->bufstk + 1)->sp)


/* various flag values for state_t; pre-shifted for efficiency and
   to enable set/test/clear multiple flags in a single call */
#define CSPROF_EXC_HANDLING (1 << 0)   /* true while exception processing */
#define CSPROF_TAIL_CALL (1 << 1)      /* true if we're unable to swap tramp;
                                          usually this only happens when tail
                                          calls occur */
#define CSPROF_THRU_TRAMP (1 << 2)     /* true if we've been through a trampoline
                                          since the last signal we hit */
#define CSPROF_BOGUS_CRD (1 << 3)      /* true if we discovered from
                                          the unwind process that the
                                          current context's CRD is
                                          totally bogus
                                          (e.g. NON_CONTEXT for a
                                          perfectly ordinary stack
                                          frame procedure...) */
/* true if we discovered during the unwinding that the return address has
   already been reloaded from the stack */
#define CSPROF_EPILOGUE_RA_RELOADED (1 << 4)
/* true if we discovered during the unwinding that the stack/frame pointer
   has been resest */
#define CSPROF_EPILOGUE_SP_RESET (1 << 5)
/* true if we received a signal whilst executing the trampoline */
#define CSPROF_SIGNALED_DURING_TRAMPOLINE (1 << 6)
/* true if this malloc has realloc in its call chain */
#define CSPROF_MALLOCING_DURING_REALLOC (1 << 7)

static inline int
csprof_state_flag_isset(state_t *state, unsigned int flag)
{
    extern int s1;
    unsigned int state_flags = state->flags;

    s1 = s1 + 1;
    TMSG(STATE,"state flag isset: %x",state_flags & flag);
    return state_flags & flag;
}

static inline void
csprof_state_flag_set(state_t *state, unsigned int flag)
{
    state->flags = state->flags | flag;
}

static inline void
csprof_state_flag_clear(state_t *state, unsigned int flag)
{
    state->flags = state->flags & (~flag);
}

#endif // STATE_H
