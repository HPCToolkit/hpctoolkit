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
// Copyright ((c)) 2002-2011, Rice University
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

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

//************************ libmonitor Include Files *************************

#include <monitor.h>

//*************************** User Include Files ****************************

#include "newmem.h"
#include "epoch.h"
#include "handling_sample.h"

#include "thread_data.h"

#include <lush/lush-pthread.h>
#include <messages/messages.h>
#include <trampoline/common/trampoline.h>
#include <memory/mmap.h>

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


thread_data_t* (*hpcrun_get_thread_data)(void)  = &local_td;
bool           (*hpcrun_td_avail)(void)         = &local_true;

void
hpcrun_unthreaded_data(void)
{
  hpcrun_get_thread_data = &local_td;
  hpcrun_td_avail        = &local_true;

}


enum _local_int_const {
  BACKTRACE_INIT_SZ     = 32,
  NEW_BACKTRACE_INIT_SZ = 32
};


void
hpcrun_thread_data_init(int id, cct_ctxt_t* thr_ctxt, int is_child)
{
  hpcrun_meminfo_t memstore;
  thread_data_t* td = hpcrun_get_thread_data();

  // ----------------------------------------
  // memstore for hpcrun_malloc()
  // ----------------------------------------

  // Wipe the thread data with a bogus bit pattern, but save the
  // memstore so we can reuse it in the child after fork.  This must
  // come first.
  td->suspend_sampling = 1;
  memstore = td->memstore;
  memset(td, 0xfe, sizeof(thread_data_t));
  td->memstore = memstore;
  hpcrun_make_memstore(&td->memstore, is_child);
  td->mem_low = 0;

  // ----------------------------------------
  // normalized thread id (monitor-generated)
  // ----------------------------------------
  td->id = id;

  // ----------------------------------------
  // sample sources
  // ----------------------------------------
  memset(&td->eventSet, 0, sizeof(td->eventSet));
  memset(&td->ss_state, UNINIT, sizeof(td->ss_state));

  td->last_time_us = 0;

  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  td->epoch = hpcrun_malloc(sizeof(epoch_t));
  td->epoch->csdata_ctxt = copy_thr_ctxt(thr_ctxt);

  // ----------------------------------------
  // cct2metrics map: associate a metric_set with
  //                  a cct node
  // ----------------------------------------
  hpcrun_cct2metrics_init(&(td->cct2metrics_map));

  // ----------------------------------------
  // backtrace buffer
  // ----------------------------------------
  hpcrun_bt_init(&(td->bt), NEW_BACKTRACE_INIT_SZ);

  // ----------------------------------------
  // trampoline
  // ----------------------------------------
  td->tramp_present     = false;
  td->tramp_retn_addr   = NULL;
  td->tramp_loc         = NULL;
  td->cached_bt         = hpcrun_malloc(sizeof(frame_t)
					* CACHED_BACKTRACE_SIZE);
  td->cached_bt_end     = td->cached_bt;          
  td->cached_bt_buf_end = td->cached_bt + CACHED_BACKTRACE_SIZE;
  td->tramp_frame       = NULL;
  td->tramp_cct_node    = NULL;

  // ----------------------------------------
  // exception stuff
  // ----------------------------------------
  memset(&td->bad_unwind, 0, sizeof(td->bad_unwind));
  memset(&td->mem_error, 0, sizeof(td->mem_error));
  hpcrun_init_handling_sample(td, 0, id);
  td->splay_lock    = 0;
  td->fnbounds_lock = 0;

  // N.B.: suspend_sampling is already set!

  // ----------------------------------------
  // Logical unwinding
  // ----------------------------------------
  lushPthr_init(&td->pthr_metrics);
  lushPthr_thread_init(&td->pthr_metrics);

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  td->trace_min_time_us = 0;
  td->trace_max_time_us = 0;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  td->hpcrun_file  = NULL;
  td->trace_file   = NULL;
  td->trace_buffer = NULL;

  // ----------------------------------------
  // debug support
  // ----------------------------------------
  td->debug1 = false;

  // ----------------------------------------
  // miscellaneous
  // ----------------------------------------
  td->inside_dlfcn = false;
}

static pthread_key_t _hpcrun_key;

void
hpcrun_init_pthread_key(void)
{
  TMSG(THREAD_SPECIFIC,"creating _hpcrun_key");
  int bad = pthread_key_create(&_hpcrun_key, NULL);
  if (bad){
    EMSG("pthread_key_create returned non-zero = %d",bad);
  }
}


void
hpcrun_set_thread_data(thread_data_t *td)
{
  TMSG(THREAD_SPECIFIC,"setting td");
  pthread_setspecific(_hpcrun_key,(void *) td);
}


void
hpcrun_set_thread0_data(void)
{
  TMSG(THREAD_SPECIFIC,"set thread0 data");
  hpcrun_set_thread_data(&_local_td);
}


thread_data_t *
hpcrun_allocate_thread_data(void)
{
  TMSG(THREAD_SPECIFIC,"malloc thread data");
  return hpcrun_mmap_anon(sizeof(thread_data_t));
}


static bool
thread_specific_td_avail(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_hpcrun_key);
  return !(ret == NULL);
}


thread_data_t *
thread_specific_td(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_hpcrun_key);
  if (!ret){
    monitor_real_abort();
  }
  return ret;
}


void
hpcrun_threaded_data(void)
{
  assert(hpcrun_get_thread_data == &local_td);
  hpcrun_get_thread_data = &thread_specific_td;
  hpcrun_td_avail        = &thread_specific_td_avail;
}

