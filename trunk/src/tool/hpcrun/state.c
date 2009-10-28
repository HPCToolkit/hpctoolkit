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
#include "epoch.h"
#include "name.h"
#include "thread_data.h"
#include "hpcrun_return_codes.h"
#include "monitor.h"
#include <trampoline/common/trampoline.h>
#include <messages/messages.h>

void
hpcrun_reset_state(state_t* state)
{
  state->next = NULL;
  TD_GET(state) = state;
}


void
hpcrun_state_init(void)
{
  TMSG(STATE,"init");
  thread_data_t* td    = hpcrun_get_thread_data();
  state_t*       state = td->state;

  hpcrun_cct_init(&(state->csdata), state->csdata_ctxt);
  state->epoch = hpcrun_get_epoch();
  state->next  = NULL;
}


state_t*
hpcrun_check_for_new_epoch(state_t* state)
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

  hpcrun_epoch_t* current = hpcrun_get_epoch();

  if(state->epoch != current) {
    TMSG(EPOCH, "Need new epoch!");
    TMSG(MALLOC," -new_epoch-");
    state_t* newstate = hpcrun_malloc(sizeof(state_t));

    TMSG(EPOCH, "check_new_epoch creating new state (new epoch/cct pair)...");

    memcpy(newstate, state, sizeof(state_t));
    hpcrun_cct_init(&newstate->csdata, newstate->csdata_ctxt);

    hpcrun_trampoline_remove();

    newstate->epoch = current;
    newstate->next  = state;

    TD_GET(state) = newstate;
    return newstate;
  }
  else {
    return state;
  }
}


int
hpcrun_state_fini(state_t *x){

  TMSG(STATE,"--Fini");
  return HPCRUN_OK;
}

void
hpcrun_state_reset(void)
{
  //
  // create a new state list:
  //  preserve current loadmap
  //  re-init cct
  // reset state list for thread to point be a list consisting of only the new state
  //
  //  N.B. the notion of 'epoch' is called 'state' in the code
  // FIXME: change the naming to reflect this
  //
  TMSG(STATE_RESET,"--started");
  state_t *state = hpcrun_get_state();
  state_t *newstate = hpcrun_malloc(sizeof(state_t));
  memcpy(newstate, state, sizeof(state_t));
  TMSG(STATE_RESET, "check new epoch = old epoch = %d", newstate->epoch == state->epoch);
  hpcrun_cct_init(&newstate->csdata, newstate->csdata_ctxt); // reset cct
  hpcrun_reset_state(newstate);
  TMSG(STATE_RESET," ==> no new state for next sample = %d", newstate->epoch == hpcrun_get_epoch());
}
