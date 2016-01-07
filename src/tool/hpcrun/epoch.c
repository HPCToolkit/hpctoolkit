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

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "env.h"
#include "epoch.h"
#include "loadmap.h"
#include "name.h"
#include "thread_data.h"
#include "hpcrun_return_codes.h"
#include "monitor.h"
#include <trampoline/common/trampoline.h>
#include <messages/messages.h>
#include <cct/cct_bundle.h>

void
hpcrun_reset_epoch(epoch_t* epoch)
{
  epoch->next = NULL;
  TD_GET(core_profile_trace_data.epoch) = epoch;
}

void
hpcrun_epoch_init(cct_ctxt_t* ctxt)
{
  TMSG(EPOCH,"init");
  thread_data_t* td    = hpcrun_get_thread_data();
  epoch_t*       epoch = td->core_profile_trace_data.epoch;

  hpcrun_cct_bundle_init(&(epoch->csdata), ctxt);

  epoch->loadmap = hpcrun_getLoadmap();
  epoch->next  = NULL;
}

epoch_t*
hpcrun_check_for_new_loadmap(epoch_t* epoch)
{
  /* ugh, nasty race condition here:

  1. shared library epoch has changed since the last profile
  signal, so we enter the if;

  2. somebody else dlclose()'s a library which holds something
  located in our backtrace.  this is not in itself a problem,
  since we don't bother doing anything on dlclose()...;

  3. somebody else (thread in step 2 or a different thread)
  dlopen()'s a new shared object, which begins an entirely
  new loadmap--one which does not include the shared object
  which resides in our backtrace;

  4. we create a new epoch which receives the loadmap from step 3,
  not step 1, which is wrong.

  attempt to take baby steps to stop this.  more drastic action
  would involve grabbing the loadmap lock, but I believe that would
  be unacceptably slow (both in the atomic instruction overhead
  and the simple fact that most programs are not frequent users
  of dl*). */

  hpcrun_loadmap_t* current = hpcrun_getLoadmap();

  if(epoch->loadmap != current) {
    TMSG(LOADMAP, "Need new loadmap!");
    TMSG(MALLOC," -new_epoch-");
    epoch_t* newepoch = hpcrun_malloc(sizeof(epoch_t));

    TMSG(EPOCH, "check_new_epoch creating new epoch (new loadmap/cct pair)...");

    memcpy(newepoch, epoch, sizeof(epoch_t));
    hpcrun_cct_bundle_init(&(epoch->csdata), (epoch->csdata).ctxt);

    hpcrun_trampoline_remove();

    newepoch->loadmap = current;
    newepoch->next  = epoch;

    TD_GET(core_profile_trace_data.epoch) = newepoch;
    return newepoch;
  }
  else {
    return epoch;
  }
}

int
hpcrun_epoch_fini(epoch_t *x){

  TMSG(EPOCH,"--Fini");
  return HPCRUN_OK;
}

void
hpcrun_epoch_reset(void)
{
  //
  // create a new epoch list:
  //  preserve current loadmap
  //  re-init cct
  // reset epoch list for thread to point be a list consisting of only the new epoch
  //
  TMSG(EPOCH_RESET,"--started");
  epoch_t *epoch = hpcrun_get_thread_epoch();
  epoch_t *newepoch = hpcrun_malloc(sizeof(epoch_t));
  memcpy(newepoch, epoch, sizeof(epoch_t));
  TMSG(EPOCH_RESET, "check new loadmap = old loadmap = %d", newepoch->loadmap == epoch->loadmap);
  hpcrun_cct_bundle_init(&(newepoch->csdata), newepoch->csdata_ctxt); // reset cct
  hpcrun_reset_epoch(newepoch);
  TMSG(EPOCH_RESET," ==> no new epoch for next sample = %d", newepoch->loadmap == hpcrun_getLoadmap());
}
