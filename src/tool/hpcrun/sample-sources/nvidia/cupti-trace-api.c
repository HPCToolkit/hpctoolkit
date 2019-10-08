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
//   cupti-trace-api.c
//
// Purpose:
//   implementation of trace handlers for NVIDIA GPUs
//  
//***************************************************************************


#include "cupti-trace-api.h"

#include <stdbool.h>

#include <tool/hpcrun/threadmgr.h>
#include <tool/hpcrun/trace.h>

#include <pthread.h>
#include <monitor.h>
#include <assert.h>

#include "cupti-context-id-map.h"
#include "cupti-channel.h"
#include "nvidia.h"


#define CUPTI_TRACE_API_DEBUG 0

#if CUPTI_TRACE_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define HPCRUN_CUPTI_TRACE_BUFFER_SIZE 100
#define HPCRUN_CUPTI_TRACE_ID (1<<30)


struct cupti_trace_s {
  uint32_t stream_id;
  pthread_t thread;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  cupti_trace_channel_t *channel;
  uint64_t count;
};


/******************************************************************************
 * static indicator variables
 *****************************************************************************/

static _Atomic(bool) cupti_stop_trace_flag;
static atomic_uint cupti_stream_id;
static atomic_uint cupti_stream_counter;

/******************************************************************************
 * trace functions
 *****************************************************************************/

void
cupti_trace_init
(
)
{
  atomic_store(&cupti_stop_trace_flag, false);
  atomic_store(&cupti_stream_counter, 0);
  atomic_store(&cupti_stream_id, 0);
}


void
cupti_trace_handle
(
 cupti_entry_trace_t *entry
)
{
  PRINT("Stream append node\n");

  static __thread bool first_pass = true;
  static __thread uint64_t stream_start = 0;
  thread_data_t *td = hpcrun_get_thread_data();
  
  // FIXME(Keren): more efficient by copying to common ancester
  cct_node_t *path = entry->node;
  cct_node_t *leaf = hpcrun_cct_insert_path_return_leaf(
    (td->core_profile_trace_data.epoch->csdata).tree_root, path);
  cct_node_t *no_thread = td->core_profile_trace_data.epoch->csdata.special_no_thread_node;

  bool append = false;
  if (first_pass) {
    // Special first node and start of the stream
    append = true;
    first_pass = false;
    stream_start = entry->start - 1;
    hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
      td->prev_dLCA, entry->start - 1);
  }

  int frequency = cupti_trace_frequency_get();
  if (frequency != -1 && append == false) {
    uint64_t cur_start = entry->start;
    uint64_t cur_end = entry->end;
    uint64_t intervals = (cur_start - stream_start - 1) / frequency + 1;
    uint64_t pivot = intervals * frequency + stream_start;
    if (pivot <= cur_end && pivot >= cur_start) {
      // only trace when the pivot is within the range
      PRINT("pivot %" PRIu64 " not in <%" PRIu64 ", %" PRIu64 "> with intervals %" PRIu64 ", frequency %" PRIu64 "\n",
        pivot, cur_start, cur_end, intervals, frequency);
      append = true;
    } 
  } else if (frequency == -1) {
    append = true;
  }

  if (append) {
    hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0,
      td->prev_dLCA, entry->start);
    hpcrun_trace_append_stream(&td->core_profile_trace_data, no_thread, 0,
      td->prev_dLCA, entry->end);
    PRINT("Write node lm_id %d lm_ip %p\n", hpcrun_cct_addr(leaf)->ip_norm.lm_id,
      hpcrun_cct_addr(leaf)->ip_norm.lm_ip);
    PRINT("Write trace start %" PRIu64 " end %" PRIu64 "\n", entry->start, entry->end);
  }
}


void *
cupti_trace_collect
(
 void *arg
)
{
  PRINT("Init trace collecting\n");

  cupti_trace_t *trace = (cupti_trace_t *)arg;
  pthread_cond_t *cond = &(trace->cond);
  pthread_mutex_t *mutex = &(trace->mutex);
  cupti_trace_channel_t *channel = trace->channel;

  // FIXME(Keren): adjust
  //!unsure of the first argument
  thread_data_t *td = NULL;
  uint32_t trace_id = HPCRUN_CUPTI_TRACE_ID - atomic_fetch_add(&cupti_stream_id, 1);
  hpcrun_threadMgr_non_compact_data_get(trace_id, NULL, &td);
  hpcrun_set_thread_data(td);

  while (!atomic_load(&cupti_stop_trace_flag)) {
    cupti_trace_channel_consume(channel);
    pthread_cond_wait(cond, mutex);
  }

  // Flush back at last
  cupti_trace_channel_consume(channel);

  epoch_t *epoch = TD_GET(core_profile_trace_data.epoch);
  hpcrun_threadMgr_data_put(epoch, td);
  atomic_fetch_add(&cupti_stream_counter, -1);

  PRINT("Finish trace collecting\n");

  return NULL;
}


cupti_trace_t *
cupti_trace_create()
{
  // Init variables
  cupti_trace_t *trace = (cupti_trace_t *)hpcrun_malloc_safe(sizeof(cupti_trace_t));

  pthread_mutex_init(&trace->mutex, NULL);
  pthread_cond_init(&trace->cond, NULL);
  trace->channel = (cupti_trace_channel_t *)hpcrun_malloc_safe(sizeof(cupti_trace_channel_t));

  // Create a new thread for the stream
  monitor_disable_new_threads();

  atomic_fetch_add(&cupti_stream_counter, 1);
  pthread_create(&trace->thread, NULL, cupti_trace_collect, trace);

  monitor_enable_new_threads();

  return trace;
}


void
cupti_trace_append(cupti_trace_t *trace, void *arg)
{
  cupti_entry_trace_t *entry_trace = (cupti_entry_trace_t *)arg;

  PRINT("Append trace start %" PRIu64 " end %" PRIu64 "\n",
    entry_trace->start, entry_trace->end); 

  cupti_trace_channel_produce(trace->channel, entry_trace->start,
    entry_trace->end, entry_trace->node);
  trace->count++;

  PRINT("Append node lm_id %d lm_ip %p\n", hpcrun_cct_addr(entry_trace->node)->ip_norm.lm_id,
    hpcrun_cct_addr(entry_trace->node)->ip_norm.lm_ip);

  // Notify the stream thread when buffer limit is reached
  if (trace->count == HPCRUN_CUPTI_TRACE_BUFFER_SIZE) {
    pthread_cond_signal(&trace->cond);
    trace->count = 0;
  }
}


static void
cupti_trace_signal(cupti_trace_t *trace, void *arg)
{
  pthread_cond_signal(&trace->cond);
}


void
cupti_trace_fini(void *arg)
{
  atomic_store(&cupti_stop_trace_flag, true);
  cupti_context_id_map_device_process(cupti_trace_signal, NULL);
  while (atomic_load(&cupti_stream_counter)) {
    cupti_context_id_map_device_process(cupti_trace_signal, NULL);
  }
}
