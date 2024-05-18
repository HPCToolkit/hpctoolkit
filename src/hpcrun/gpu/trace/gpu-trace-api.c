// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <pthread.h>
#include <unistd.h>



//******************************************************************************
// libmonitor
//******************************************************************************

#include "../../libmonitor/monitor.h"



//******************************************************************************
// local includes
//******************************************************************************

#include <stdatomic.h>
#include "../../../common/lean/mcs-lock.h"
#include "../../../common/lean/OSUtil.h"

#include "../../cct/cct.h"
#include "../../control-knob.h"
#include "../../rank.h"
#include "../../thread_data.h"
#include "../../threadmgr.h"
#include "../../trace.h"
#include "../../write_data.h"

#include "../common/gpu-monitoring.h"

#include "gpu-trace-api.h"
#include "gpu-trace-item.h"
#include "gpu-trace-channel.h"
#include "gpu-trace-channel-set.h"
#include "gpu-trace-demultiplexer.h"
#include "gpu-context-id-map.h"
#include "gpu-stream-id-map.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_tag_t {
  uint32_t device_id;
  uint32_t context_id;
  uint32_t stream_id;
} gpu_tag_t;



//******************************************************************************
// local variables
//******************************************************************************

static _Atomic(bool) stop_trace_flag;

static atomic_ullong active_streams_counter;

static atomic_ullong num_streams;

static __thread uint64_t stream_start = 0;

static mcs_lock_t lock;



//******************************************************************************
// private operations
//******************************************************************************

static void
stream_start_set
(
 uint64_t start_time
)
{
  if (!stream_start) stream_start = start_time;
}


static uint64_t
stream_start_get
(
 void
)
{
  return stream_start;
}


static int
gpu_trace_stream_id
(
 void
)
{
  // FIXME: this is a bad way to compute a stream id
  int id = 500 + atomic_fetch_add(&num_streams, 1);

  return id;
}


static void
gpu_compute_profile_name
(
 gpu_tag_t tag,
 core_profile_trace_data_t * cptd
)
{
  pms_id_t ids[IDTUPLE_MAXTYPES];
  id_tuple_t id_tuple;

  id_tuple_constructor(&id_tuple, ids, IDTUPLE_MAXTYPES);

  id_tuple_push_back(&id_tuple, IDTUPLE_COMPOSE(IDTUPLE_NODE, IDTUPLE_IDS_LOGIC_LOCAL), OSUtil_hostid(), 0);

#if 0
  if (tag.device_id != IDTUPLE_INVALID) {
    id_tuple_push_back(&id_tuple, IDTUPLE_GPUDEVICE, tag.device_id);
  }
#endif

  int rank = hpcrun_get_rank();
  if (rank >= 0) id_tuple_push_back(&id_tuple, IDTUPLE_COMPOSE(IDTUPLE_RANK, IDTUPLE_IDS_LOGIC_ONLY), rank, rank);

  id_tuple_push_back(&id_tuple, IDTUPLE_COMPOSE(IDTUPLE_GPUCONTEXT, IDTUPLE_IDS_LOGIC_ONLY), tag.context_id, tag.context_id);

  id_tuple_push_back(&id_tuple, IDTUPLE_COMPOSE(IDTUPLE_GPUSTREAM, IDTUPLE_IDS_LOGIC_ONLY), tag.stream_id, tag.stream_id);

  id_tuple_copy(&cptd->id_tuple, &id_tuple, hpcrun_malloc);
}


static thread_data_t *
gpu_trace_stream_acquire
(
 gpu_tag_t tag
)
{
  thread_data_t *td = NULL;

  atomic_fetch_add(&active_streams_counter, 1);

  int id = gpu_trace_stream_id();

  // XXX(Keren): This API calls allocate_and_init_thread_data to bind td with the current thread

  // printf("THREAD_DATA A %p %p\n", td, hpcrun_get_thread_data());
  thread_data_t *current_td = hpcrun_get_thread_data();
  hpcrun_threadMgr_data_get(id, NULL, &td, HPCRUN_CALL_TRACE, true);
  hpcrun_set_thread_data(current_td);
  // printf("THREAD_DATA B %p %p\n", td, hpcrun_get_thread_data());

  gpu_compute_profile_name(tag, &td->core_profile_trace_data);

  td->core_profile_trace_data.trace_expected_disorder = 30;

  return td;
}


static cct_node_t *
gpu_trace_cct_root
(
 thread_data_t* td
)
{
  return td->core_profile_trace_data.epoch->csdata.tree_root;
}


static cct_node_t *
gpu_trace_cct_insert_context
(
 thread_data_t* td,
 cct_node_t *path
)
{
  cct_node_t *leaf =
    hpcrun_cct_insert_path_return_leaf(gpu_trace_cct_root(td), path);

  return leaf;
}


static void
gpu_trace_stream_append
(
 thread_data_t* td,
 cct_node_t *leaf,
 uint64_t time
)
{
  hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0,
                           td->prev_dLCA, time);
}

static uint64_t
gpu_trace_start_adjust
(
 thread_data_t* td,
 uint64_t start,
 uint64_t end
)
{
  uint64_t last_end = td->gpu_trace_prev_time;

  if(start < last_end) {
    // If we have a hardware measurement error (Power9),
    // set the offset as the end of the last activity
    start = last_end + 1;
  }

  td->gpu_trace_prev_time = end;

  return start;
}


static cct_node_t *
gpu_trace_cct_no_activity
(
 thread_data_t* td
)
{
  cct_node_t *no_activity =
    hpcrun_cct_bundle_get_no_activity_node(&(td->core_profile_trace_data.epoch->csdata));

  return no_activity;
}


static void
gpu_trace_channel_release
(
  gpu_trace_channel_t *channel,
  void *arg
)
{
  thread_data_t *td = gpu_trace_channel_get_thread_data(channel);
  hpcrun_write_profile_data(&td->core_profile_trace_data);
  hpcrun_trace_close(&td->core_profile_trace_data);

  atomic_fetch_add(&active_streams_counter, -1);
}

static void
consume_trace_item(gpu_trace_item_t *trace_item, thread_data_t *thread_data, void *arg) {
  cct_node_t *no_activity = arg;
  cct_node_t *call_path = trace_item->call_path_leaf;
  uint64_t start_time = trace_item->start;
  uint64_t end_time = trace_item->end;

  cct_node_t *leaf = gpu_trace_cct_insert_context(thread_data, call_path);

  uint64_t start = start_time;
  uint64_t end   = end_time;

  stream_start_set(start_time);

  start = gpu_trace_start_adjust(thread_data, start, end);

  uint32_t frequency = gpu_monitoring_trace_sample_frequency_get();

  bool append = false;

  if (frequency != -1) {
    uint64_t cur_start = start_time;
    uint64_t cur_end = end_time;
    uint64_t intervals = (cur_start - stream_start_get() - 1) / frequency + 1;
    uint64_t pivot = intervals * frequency + stream_start;

    if (pivot <= cur_end && pivot >= cur_start) {
      // only trace when the pivot is within the range
      PRINT("pivot %" PRIu64 " not in <%" PRIu64 ", %" PRIu64
            "> with intervals %" PRIu64 ", frequency %" PRIu32 "\n",
            pivot, cur_start, cur_end, intervals, frequency);
      append = true;
    }
  } else {
    append = true;
  }

  if (append) {
    gpu_trace_stream_append(thread_data, leaf, start);

    // note: adding 1 to end makes sense. however, with AMD OMPT, this
    // causes adjacent events to share a timestamp. so, don't add 1.
    gpu_trace_stream_append(thread_data, no_activity, end);

    PRINT("%p Append trace activity [%lu, %lu)\n", thread_data, start, end);
  }
}


static void
consume_trace_channel
(
  gpu_trace_channel_t *channel,
  void *arg
)
{
  thread_data_t *thread_data = gpu_trace_channel_get_thread_data(channel);
  hpcrun_set_thread_data(thread_data);
  cct_node_t *no_activity = gpu_trace_cct_no_activity(thread_data);

  gpu_trace_channel_receive_all(channel, consume_trace_item, no_activity);
}


static void
stream_iter_fn
(
 gpu_trace_channel_t *channel,
 void *arg
)
{
  gpu_trace_item_t *trace_item = arg;
  gpu_trace_channel_send(channel, trace_item);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_trace_init
(
 void
)
{
  atomic_store(&stop_trace_flag, false);
  atomic_store(&active_streams_counter, 0);
  atomic_store(&num_streams, 0);
}


void
gpu_trace_fini
(
 void *arg,
 int how
)
{
  PRINT("gpu_trace_fini called\n");

  // trace finalization is idempotent
  if (atomic_load(&stop_trace_flag) != true) {

    atomic_store(&stop_trace_flag, true);

    gpu_trace_demultiplexer_notify();

    while (atomic_load(&active_streams_counter));
  }
}


// Tracing thread
void *
gpu_trace_record_thread_fn
(
 void * args
)
{
  gpu_trace_channel_set_t *channel_set = (gpu_trace_channel_set_t *) args;

  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

  while (!atomic_load(&stop_trace_flag)) {
    gpu_trace_channel_set_await(channel_set);
    gpu_trace_channel_set_apply(channel_set, consume_trace_channel, NULL);
  }

  gpu_trace_channel_set_apply(channel_set, consume_trace_channel, NULL);
  gpu_trace_channel_set_apply(channel_set, gpu_trace_channel_release, NULL);

  return NULL;
}


void
gpu_trace_process_stream
(
 uint32_t device_id,
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_item_t *trace_item
)
{
  if (!hpcrun_trace_isactive()) {
    return;
  }

  mcs_node_t node;
  mcs_lock(&lock, &node);

  /* Create context if it does not exist */
  gpu_context_id_map_entry_t *context_entry = gpu_context_id_map_lookup(context_id);
  if (context_entry == NULL) {
    context_entry = gpu_context_id_map_entry_new(context_id);
    gpu_context_id_map_insert_entry(context_entry);
  }

  /* Create stream if it does not exist*/
  gpu_stream_id_map_entry_t **streams = gpu_context_id_map_entry_get_streams(context_entry);
  gpu_stream_id_map_entry_t *stream_entry = gpu_stream_id_map_lookup(streams, stream_id);
  if (stream_entry == NULL) {
    stream_entry = gpu_stream_id_map_entry_new(stream_id);
    gpu_stream_id_map_insert_entry(streams, stream_entry);

    gpu_tag_t tag = {
      .context_id = context_id,
      .device_id = device_id,
      .stream_id = stream_id
    };
    gpu_trace_channel_t *channel = gpu_trace_channel_new(gpu_trace_stream_acquire(tag));
    gpu_trace_demultiplexer_push(channel);
    gpu_stream_id_map_entry_set_channel(stream_entry, channel);
  }

  /* Send trace_item to channel */
  gpu_trace_channel_t *channel = gpu_stream_id_map_entry_get_channel(stream_entry);
  gpu_context_id_map_adjust_times(context_entry, trace_item);
  gpu_trace_channel_send(channel, trace_item);

  mcs_unlock(&lock, &node);
}


void
gpu_trace_process_context
(
 uint32_t context_id,
 gpu_trace_item_t *trace_item
)
{
  if (!hpcrun_trace_isactive()) {
    return;
  }

  mcs_node_t node;
  mcs_lock(&lock, &node);

  gpu_context_id_map_entry_t *entry = gpu_context_id_map_lookup(context_id);

  if (entry == NULL) {
    mcs_unlock(&lock, &node);
    return;
  }
  gpu_context_id_map_adjust_times(entry, trace_item);

  gpu_stream_id_map_entry_t **streams = gpu_context_id_map_entry_get_streams(entry);
  gpu_stream_id_map_for_each(streams, stream_iter_fn, trace_item);

  mcs_unlock(&lock, &node);
}


// This is a workaround interface to align CPU & GPU timestamps.
// Currently, only rocm support uses this.
void
gpu_set_cpu_gpu_timestamp
(
 uint64_t t1,
 uint64_t t2
)
{
  gpu_context_id_map_set_cpu_gpu_timestamp(t1, t2);
}
