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
// Copyright ((c)) 2002-2022, Rice University
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
// Created by ax4 on 8/5/19.
//
#include "stream-tracing.h"

#include "gpu-context-stream-id-map.h"
#include "tool/hpcrun/threadmgr.h"

static atomic_ullong stream_id;

void stream_tracing_init(

) {
  atomic_store(&stop_streams, 0);
  atomic_store(&stream_counter, 0);
  atomic_store(&stream_id, 0);
}

void* stream_data_collecting(void* arg) {
  stream_thread_data_t* thread_args = (stream_thread_data_t*)arg;
  stream_activity_data_t_producer_wfq_t* wfq = thread_args->wfq;
  pthread_cond_t* cond = thread_args->cond;
  pthread_mutex_t* mutex = thread_args->mutex;
  bool first_pass = true;
  stream_activity_data_elem* elem;

  thread_data_t* td = NULL;
  //! unsure of the first argument
  hpcrun_threadMgr_non_compact_data_get(500 + atomic_fetch_add(&stream_id, 1), NULL, &td, true);
  hpcrun_set_thread_data(td);

  while (!atomic_load(&stop_streams)) {
    elem = stream_activity_data_producer_wfq_dequeue(wfq);
    if (elem) {
      cct_node_t* path = elem->activity_data.node;
      cct_node_t* leaf = hpcrun_cct_insert_path_return_leaf(
          (td->core_profile_trace_data.epoch->csdata).tree_root, path);
      cct_node_t* no_thread = td->core_profile_trace_data.epoch->csdata.special_no_thread_node;
      if (first_pass) {
        first_pass = false;
        hpcrun_trace_append_STREAM(
            &td->core_profile_trace_data, no_thread, 0, td->prev_dLCA,
            elem->activity_data.start / 1000 - 1);
      }
      hpcrun_trace_append_STREAM(
          &td->core_profile_trace_data, leaf, 0, td->prev_dLCA, elem->activity_data.start / 1000);
      hpcrun_trace_append_STREAM(
          &td->core_profile_trace_data, no_thread, 0, td->prev_dLCA,
          elem->activity_data.end / 1000 + 1);
    } else {
      pthread_cond_wait(cond, mutex);
    }
  }
  while ((elem = stream_activity_data_producer_wfq_dequeue(wfq))) {
    cct_node_t* path = elem->activity_data.node;
    cct_node_t* leaf = hpcrun_cct_insert_path_return_leaf(
        (td->core_profile_trace_data.epoch->csdata).tree_root, path);
    cct_node_t* no_thread = td->core_profile_trace_data.epoch->csdata.special_no_thread_node;
    if (first_pass) {
      first_pass = false;
      hpcrun_trace_append_STREAM(
          &td->core_profile_trace_data, no_thread, 0, td->prev_dLCA,
          elem->activity_data.start / 1000 - 1);
    }
    hpcrun_trace_append_STREAM(
        &td->core_profile_trace_data, leaf, 0, td->prev_dLCA, elem->activity_data.start / 1000);
    hpcrun_trace_append_STREAM(
        &td->core_profile_trace_data, no_thread, 0, td->prev_dLCA,
        elem->activity_data.end / 1000 + 1);
  }
  epoch_t* epoch = TD_GET(core_profile_trace_data.epoch);
  hpcrun_threadMgr_data_put(epoch, td);
  atomic_fetch_add(&stream_counter, -1);
  return NULL;
}

void stream_tracing_fini(

) {
  atomic_store(&stop_streams, 1);
  signal_all_streams();
  while (atomic_load(&stream_counter))
    ;
}
