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
// Copyright ((c)) 2002-2019, Rice University
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
//   cupti-stream-trace.c
//
// Purpose:
//   implementation of activity trace for NVIDIA GPUs
//  
//***************************************************************************


#include "cupti-stream-trace.h"

#include <tool/hpcrun/threadmgr.h>
#include <tool/hpcrun/trace.h>

#include <pthread.h>
#include <monitor.h>

#include "cupti-context-stream-id-map.h"

#define CUPTI_STREAM_DEBUG 0

#if CUPTI_STREAM_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define HPCRUN_CUPTI_TRACE_BUFFER_SIZE 100
#define HPCRUN_CUPTI_TRACE_ID (1<<31 - 1)


typedef struct {
  cct_node_t *node;
  uint64_t start;
  uint64_t end;
} stream_activity_data_t;

typedef struct {
  producer_wfq_element_ptr_t next;
  stream_activity_data_t activity_data;
} typed_producer_wfq_elem(stream_activity_data_t);

typedef producer_wfq_t typed_producer_wfq(stream_activity_data_t);

struct stream_trace_s {
  pthread_t thread;
  producer_wfq_t wfq;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  uint64_t count;
};

typedef typed_producer_wfq_elem(stream_activity_data_t) stream_activity_data_elem_t;

/******************************************************************************
 * static indicator variables
 *****************************************************************************/

static _Atomic(bool) cupti_stop_streams_flag;
static atomic_ullong cupti_stream_id;
static atomic_ullong cupti_stream_counter;

/******************************************************************************
 * wfq functions
 *****************************************************************************/

typed_producer_wfq_declare(stream_activity_data_t)

typed_producer_wfq_impl(stream_activity_data_t)

#define stream_activity_data_producer_wfq_enqueue typed_producer_wfq_enqueue(stream_activity_data_t)
#define stream_activity_data_producer_wfq_dequeue typed_producer_wfq_dequeue(stream_activity_data_t)

/******************************************************************************
 * stream trace functions
 *****************************************************************************/

void
cupti_stream_trace_init
(
)
{
  atomic_store(&cupti_stop_streams_flag, false);
  atomic_store(&cupti_stream_counter, 0);
  atomic_store(&cupti_stream_id, 0);
}


void *
cupti_stream_trace_collect
(
 void *arg
)
{
  PRINT("Init stream collecting");

  stream_trace_t *stream_trace = (stream_trace_t *)arg;
  producer_wfq_t *wfq = &stream_trace->wfq;
  pthread_cond_t *cond = &stream_trace->cond;
  pthread_mutex_t *mutex = &stream_trace->mutex;
  bool first_pass = true;
  stream_activity_data_elem_t *elem = NULL;

  // FIXME(Keren): adjust
  //!unsure of the first argument
  thread_data_t *td = NULL;
  uint32_t trace_id = HPCRUN_CUPTI_TRACE_ID - atomic_fetch_add(&cupti_stream_id, 1);
  hpcrun_threadMgr_non_compact_data_get(trace_id, NULL, &td);
  hpcrun_set_thread_data(td);

  while (!atomic_load(&cupti_stop_streams_flag)) {
    elem = stream_activity_data_producer_wfq_dequeue(wfq);
    if (elem) {
      cct_node_t *path = elem->activity_data.node;
      cct_node_t *leaf = hpcrun_cct_insert_path_return_leaf(
        (td->core_profile_trace_data.epoch->csdata).tree_root, path);
      cct_node_t *no_thread = td->core_profile_trace_data.epoch->csdata.special_no_thread_node;

      PRINT("Stream append node\n");
      if (first_pass) {
        first_pass = false;
        hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
          td->prev_dLCA, elem->activity_data.start/1000 - 1);
      }
      hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0,
        td->prev_dLCA, elem->activity_data.start/1000);
      hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
        td->prev_dLCA, elem->activity_data.end/1000 + 1);
    } else {
      pthread_cond_wait(cond, mutex);
    }
  }

  // Flush back
  while ((elem = stream_activity_data_producer_wfq_dequeue(wfq))) {
    cct_node_t *path = elem->activity_data.node;
    cct_node_t *leaf = hpcrun_cct_insert_path_return_leaf((td->core_profile_trace_data.epoch->csdata).tree_root, path);
    cct_node_t *no_thread = td->core_profile_trace_data.epoch->csdata.special_no_thread_node;

    PRINT("Stream append node\n");
    if (first_pass) {
      first_pass = false;
      hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
        td->prev_dLCA, elem->activity_data.start/1000 - 1);
    }
    hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0,
      td->prev_dLCA, elem->activity_data.start/1000);
    hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
      td->prev_dLCA, elem->activity_data.end/1000 + 1);
  }

  epoch_t *epoch = TD_GET(core_profile_trace_data.epoch);
  hpcrun_threadMgr_data_put(epoch, td);
  atomic_fetch_add(&cupti_stream_counter, -1);

  PRINT("Finish stream collecting");

  return NULL;
}


stream_trace_t *
cupti_stream_trace_create()
{
  // Init variables
  stream_trace_t *stream_trace = (stream_trace_t *)hpcrun_malloc_safe(sizeof(stream_trace_t));

  producer_wfq_init(&stream_trace->wfq);
  pthread_mutex_init(&stream_trace->mutex, NULL);
  pthread_cond_init(&stream_trace->cond, NULL);

  // Create a new thread for the stream
  monitor_disable_new_threads();

  atomic_fetch_add(&cupti_stream_counter, 1);
  pthread_create(&stream_trace->thread, NULL, cupti_stream_trace_collect, stream_trace);

  monitor_enable_new_threads();

  return stream_trace;
}


static stream_activity_data_elem_t *
stream_activity_data_elem_new(uint64_t start, uint64_t end, cct_node_t *cct_node)
{
  stream_activity_data_elem_t *elem = (stream_activity_data_elem_t *)
    hpcrun_malloc_safe(sizeof(stream_activity_data_elem_t));

  elem->activity_data.node = cct_node;
  elem->activity_data.start = start;
  elem->activity_data.end = end;
  producer_wfq_ptr_set(&elem->next, NULL);

  return elem;
}


void
cupti_stream_trace_append(stream_trace_t *stream_trace, uint64_t start, uint64_t end, cct_node_t *cct_node)
{
  stream_activity_data_elem_t *elem = stream_activity_data_elem_new(start, end, cct_node);
  stream_activity_data_t_producer_wfq_enqueue(&stream_trace->wfq, elem);
  stream_trace->count++;

  // Notify the stream thread when buffer limit is reached
  if (stream_trace->count == HPCRUN_CUPTI_TRACE_BUFFER_SIZE) {
    pthread_cond_signal(&stream_trace->cond);
    stream_trace->count = 0;
  }
}


static void
cupti_stream_trace_signal(stream_trace_t *stream_trace)
{
  pthread_cond_signal(&stream_trace->cond);
}


void
cupti_stream_trace_fini(void *arg)
{
  atomic_store(&cupti_stop_streams_flag, true);
  cupti_context_stream_id_map_process(cupti_stream_trace_signal);
  while (atomic_load(&cupti_stream_counter));
}
