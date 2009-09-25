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

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "env.h"
#include "state.h"
#include "csproflib_private.h"
#include "epoch.h"
#include "name.h"
#include "thread_data.h"
#include "hpcrun_return_codes.h"
#include "monitor.h"

#include <messages/messages.h>



void
hpcrun_reset_state(state_t* state)
{
  state->next = NULL;
  TD_GET(state) = state;
}

void
csprof_set_state(state_t *state)
{
  TMSG(STATE," --Set");
  state->next = TD_GET(state);
  TD_GET(state) = state;
}

int
csprof_state_init(state_t *x)
{
  /* ia64 Linux has this function return a `long int', which is a 64-bit
     integer.  Tru64 Unix returns an `int'.  it probably won't hurt us
     if we get truncated on ia64, right? */

  TMSG(STATE,"--Init");
  memset(x, 0, sizeof(*x));

  return HPCRUN_OK;
}

/* csprof_state_alloc: Special initialization for items stored in
   private memory.  Private memory must be initialized!  Returns
   HPCRUN_OK upon success; HPCRUN_ERR on error. */
int
csprof_state_alloc(state_t *x, lush_cct_ctxt_t* thr_ctxt)
{
  TMSG(STATE,"--Alloc");
  csprof_cct__init(&x->csdata, thr_ctxt);

  x->epoch = hpcrun_get_epoch();
  x->csdata_ctxt = thr_ctxt;

#ifdef CSPROF_TRAMPOLINE_BACKEND
  x->pool = csprof_list_pool_new(32);
#if defined(CSPROF_LIST_BACKTRACE_CACHE)
  x->backtrace = csprof_list_new(x->pool);
#else
  TMSG(STATE," state_alloc TRAMP");
  x->btbuf = csprof_malloc(sizeof(hpcrun_frame_t)*32);
  x->bufend = x->btbuf + 32;
  x->bufstk = x->bufend;
  x->treenode = NULL;
#endif
#else
#if defined(CSPROF_LIST_BACKTRACE_CACHE)
  x->backtrace = csprof_list_new(x->pool);
#else
  TMSG(STATE," state_alloc btbuf (no TRAMP)");
  x->btbuf = csprof_malloc(sizeof(hpcrun_frame_t) * CSPROF_BACKTRACE_CACHE_INIT_SZ);
  x->bufend = x->btbuf + CSPROF_BACKTRACE_CACHE_INIT_SZ;
  x->bufstk = x->bufend;
  x->treenode = NULL;
#endif
#endif

  return HPCRUN_OK;
}

state_t*
csprof_check_for_new_epoch(state_t *state)
{
  /* ugh, nasty race condition here:

  1. shared library state has changed since the last profile
  signal, so we enter the if;

  2. somebody else dlclose()'s a library which holds something
  located in our backtrace.  this is not in itself a problem,
  since we don't bother doing anything on dlclose()...;

  3. somebody else (thread in step 2 or a different thread)
  dlopen()'s a new shared object, which begins an entirely
  new epoch--one which does not include the shared object
  which resides in our backtrace;

  4. we create a new state which receives the epoch from step 3,
  not step 1, which is wrong.

  attempt to take baby steps to stop this.  more drastic action
  would involve grabbing the epoch lock, but I believe that would
  be unacceptably slow (both in the atomic instruction overhead
  and the simple fact that most programs are not frequent users
  of dl*). */

  TMSG(EPOCH_CHK,"Likely need new cct");

  hpcrun_epoch_t *current = hpcrun_get_epoch();

  if(state->epoch != current) {
    TMSG(MALLOC," -new_epoch-");
    state_t *newstate = csprof_malloc(sizeof(state_t));

    TMSG(EPOCH, "check_new_epoch creating new state (new epoch/cct pair)...");

    /* we don't have to go through the usual csprof_state_{init,alloc}
       business here because most of the stuff we want is already
       in `state' */
    memcpy(newstate, state, sizeof(state_t));

    /* we do have to reinitialize the tree, though */
    csprof_cct__init(&newstate->csdata, newstate->csdata_ctxt);

    /* and reinsert backtraces */
    if(newstate->bufend - newstate->bufstk != 0) {
      TMSG(EPOCH_CHK,"New backtraces must be reinserted");
      newstate->treenode = NULL;
      csprof_state_insert_backtrace(newstate, 0, /* pick one */
                                    newstate->bufend - 1,
                                    newstate->bufstk,
				    (cct_metric_data_t){ .i = 0 });
    }

    /* and inform the state about its epoch */
    newstate->epoch = current;

    /* and finally, set the new state */
    csprof_set_state(newstate);
    DISABLE(EPOCH_CHK);
    return newstate;
  }
  else {
    return state;
  }
}

int
csprof_state_fini(state_t *x){

  TMSG(STATE,"--Fini");
  return HPCRUN_OK;
}

csprof_cct_node_t*
csprof_state_insert_backtrace(state_t *state, int metric_id,
			      hpcrun_frame_t *path_beg,
			      hpcrun_frame_t *path_end,
			      cct_metric_data_t increment)
{
  csprof_cct_node_t* n;
  n = csprof_cct_insert_backtrace(&state->csdata, state->treenode,
				  metric_id, path_beg, path_end, increment);

  TMSG(CCT, "Treenode is %p", n);
  
  state->treenode = n;
  return n;
}

hpcrun_frame_t* 
csprof_state_expand_buffer(state_t *state, hpcrun_frame_t *unwind){
  /* how big is the current buffer? */
  size_t sz = state->bufend - state->btbuf;
  size_t newsz = sz*2;
  /* how big is the current backtrace? */
  size_t btsz = state->bufend - state->bufstk;
  /* how big is the backtrace we're recording? */
  size_t recsz = unwind - state->btbuf;
  /* get new buffer */
  TMSG(STATE," state_expand_buffer");
  hpcrun_frame_t *newbt = csprof_malloc(newsz*sizeof(hpcrun_frame_t));

  if(state->bufstk > state->bufend) {
    EMSG("Invariant bufstk > bufend violated");
    monitor_real_abort();
  }

  /* copy frames from old to new */
  memcpy(newbt, state->btbuf, recsz*sizeof(hpcrun_frame_t));
  memcpy(newbt+newsz-btsz, state->bufend-btsz, btsz*sizeof(hpcrun_frame_t));

  /* setup new pointers */
  state->btbuf = newbt;
  state->bufend = newbt+newsz;
  state->bufstk = newbt+newsz-btsz;

  /* return new unwind pointer */
  return newbt+recsz;
}

/* csprof_state_free: Special finalization for items stored in
   private memory.  Private memory must be initialized!  Returns
   HPCRUN_OK upon success; HPCRUN_ERR on error. */
int
csprof_state_free(state_t *x){
  csprof_cct__fini(&x->csdata);

  // no need to free memory

  return HPCRUN_OK;
}
