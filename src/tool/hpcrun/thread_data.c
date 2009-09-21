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

//
//

//************************* System Include Files ****************************

#include <stdlib.h>
#include <assert.h>

//************************ libmonitor Include Files *************************

#include <monitor.h>

//*************************** User Include Files ****************************

#include "newmem.h"
#include "state.h"
#include "handling_sample.h"

#include "thread_data.h"

#include <lush/lush-pthread.h>
#include <messages/messages.h>

//***************************************************************************


static thread_data_t _local_td;

static thread_data_t *
local_td(void)
{
  return &_local_td;
}

static bool
local_true(void)
{
  return true;
}

thread_data_t *(*csprof_get_thread_data)(void) = &local_td;
bool          (*csprof_td_avail)(void)         = &local_true;

void
csprof_unthreaded_data(void)
{
  csprof_get_thread_data = &local_td;
  csprof_td_avail        = &local_true;

}


thread_data_t*
hpcrun_thread_data_new(void)
{
  NMSG(THREAD_SPECIFIC,"new thread specific data");
  thread_data_t *td = csprof_get_thread_data();

  td->suspend_sampling            = 1; // protect against spurious signals

  // initialize thread_data with known bogus bit pattern so that missing
  // initializations will be apparent.
  memset(td, 0xfe, sizeof(thread_data_t));

  return td;
}

void
hpcrun_thread_memory_init(void)
{
  thread_data_t* td = csprof_get_thread_data();
  hpcrun_make_memstore(&td->memstore);
}

void
hpcrun_thread_data_init(int id, lush_cct_ctxt_t* thr_ctxt)
{
  thread_data_t* td = csprof_get_thread_data();
  csprof_init_handling_sample(td, 0, id);

  td->id                          = id;
  td->mem_low                     = 0;
  td->state                       = NULL;

  // hpcrun file
  td->hpcrun_file                 = NULL;

  // locks
  td->fnbounds_lock               = 0;
  td->splay_lock                  = 0;

  // trampolines
  td->tramp_present               = false;
  td->tramp_retn_addr             = NULL;
  td->tramp_loc                   = NULL;

  td->trace_file                  = NULL;
  td->last_time_us                = 0;

  lushPthr_init(&td->pthr_metrics);
  lushPthr_thread_init(&td->pthr_metrics);

  memset(&td->bad_unwind, 0, sizeof(td->bad_unwind));
  memset(&td->mem_error, 0, sizeof(td->mem_error));
  memset(&td->eventSet, 0, sizeof(td->eventSet));
  memset(&td->ss_state, UNINIT, sizeof(td->ss_state));

  TMSG(THREAD_SPECIFIC," thread_data_init state");
  csprof_state_t *state = csprof_malloc(sizeof(csprof_state_t));

  thr_ctxt = copy_thr_ctxt(thr_ctxt);

  csprof_set_state(state);
  csprof_state_init(state);
  csprof_state_alloc(state, thr_ctxt);
}

#ifdef CSPROF_THREADS
#include <pthread.h>

static pthread_key_t _csprof_key;

void
csprof_init_pthread_key(void)
{
  TMSG(THREAD_SPECIFIC,"creating _csprof_key");
  int bad = pthread_key_create(&_csprof_key, NULL);
  if (bad){
    EMSG("pthread_key_create returned non-zero = %d",bad);
  }
}


void
csprof_set_thread_data(thread_data_t *td)
{
  NMSG(THREAD_SPECIFIC,"setting td");
  pthread_setspecific(_csprof_key,(void *) td);
}


void
csprof_set_thread0_data(void)
{
  TMSG(THREAD_SPECIFIC,"set thread0 data");
  csprof_set_thread_data(&_local_td);
}

// FIXME: use csprof_malloc ??
thread_data_t *
csprof_allocate_thread_data(void)
{
  NMSG(THREAD_SPECIFIC,"malloc thread data");
  return malloc(sizeof(thread_data_t));
}

static bool
thread_specific_td_avail(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_csprof_key);
  return !(ret == NULL);
}

thread_data_t *
thread_specific_td(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_csprof_key);
  if (!ret){
    monitor_real_abort();
  }
  return ret;
}

void
csprof_threaded_data(void)
{
  assert(csprof_get_thread_data == &local_td);
  csprof_get_thread_data = &thread_specific_td;
  csprof_td_avail        = &thread_specific_td_avail;
}
#endif // defined(CSPROF_THREADS)
