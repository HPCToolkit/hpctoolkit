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
// Copyright ((c)) 2002-2021, Rice University
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

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>
#include <hpcrun/trace.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-trace-item.h>
#include <hpcrun/gpu/gpu-context-id-map.h>
#include <lib/prof-lean/stdatomic.h>

#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"
#include "gpu-range.h"
#include "gpu-metrics.h"

//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"

//******************************************************************************
// private operations
//******************************************************************************

static void
gpu_context_stream_trace
(
 uint32_t device_id,
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_item_t *ti
)
{
  if (hpcrun_trace_isactive()) {
    gpu_context_id_map_stream_process(device_id, context_id, stream_id, gpu_trace_produce, ti);
  }
}

//******************************************************************************
// gpu operations process
//******************************************************************************

static void
gpu_memcpy_process
(
 gpu_activity_t *activity,
 gpu_activity_channel_t *channel
)
{
  assert(activity->cct_node != NULL);

  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.memcpy.submit_time,
                         activity->details.memcpy.start,
                         activity->details.memcpy.end,
                         activity->cct_node);

  gpu_context_stream_trace(activity->details.memcpy.device_id,
                           activity->details.memcpy.context_id,
                           activity->details.memcpy.stream_id,
                           &entry_trace);

  gpu_activity_channel_produce(channel, activity);
}


static void
gpu_kernel_process
(
 gpu_activity_t *activity,
 gpu_activity_channel_t *channel
)
{
  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.kernel.submit_time,
                         activity->details.kernel.start,
                         activity->details.kernel.end,
                         activity->cct_node);

  gpu_context_stream_trace(activity->details.kernel.device_id,
                           activity->details.kernel.context_id,
                           activity->details.kernel.stream_id,
                           &entry_trace);

  gpu_activity_channel_produce(channel, activity);

  TMSG(GPU_OPERATION, "Kernel execution range %u\n", activity->range_id);
  TMSG(GPU_OPERATION, "Kernel execution cct_node %p\n", activity->cct_node);
  TMSG(GPU_OPERATION, "Kernel execution deviceId %u\n", activity->details.kernel.device_id);
}


static void
gpu_synchronization_process
(
 gpu_activity_t *activity,
 gpu_activity_channel_t *channel
)
{
  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.synchronization.submit_time,
                         activity->details.synchronization.start,
                         activity->details.synchronization.end,
                         activity->cct_node);

  TMSG(GPU_OPERATION, "Sync range range %u\n", activity->range_id);

  if (activity->kind == GPU_ACTIVITY_SYNCHRONIZATION) {
    uint32_t context_id = activity->details.synchronization.context_id;
    uint32_t stream_id = activity->details.synchronization.stream_id;

    switch (activity->details.synchronization.syncKind) {
      case GPU_SYNC_STREAM:
      case GPU_SYNC_STREAM_EVENT_WAIT:
        // Insert a event for a specific stream
        gpu_context_id_map_stream_process(IDTUPLE_INVALID, context_id, stream_id, gpu_trace_produce, &entry_trace);
        break;
      case GPU_SYNC_CONTEXT:
        // Insert events for all current active streams
        // TODO(Keren): What if the stream is created
        gpu_context_id_map_context_process(IDTUPLE_INVALID, context_id, gpu_trace_produce, &entry_trace);
        break;
      case GPU_SYNC_EVENT:
        gpu_context_id_map_stream_process(IDTUPLE_INVALID, context_id, stream_id, gpu_trace_produce, &entry_trace);
        break;
      default:
        // invalid
        TMSG(GPU_OPERATION, "Invalid synchronization\n");
    }
  }

  gpu_activity_channel_produce(channel, activity);
}


void
gpu_pc_sampling_info2_process
(
 gpu_activity_t *activity
)
{
  uint32_t range_id = activity->range_id;
  gpu_pc_sampling_info2_t *pc_sampling_info2 = &activity->details.pc_sampling_info2;

  uint32_t context_id = pc_sampling_info2->context_id;
  void *pc_sampling_data = pc_sampling_info2->pc_sampling_data;
  uint32_t period = pc_sampling_info2->samplingPeriodInCycles;
  uint64_t total_num_pcs = pc_sampling_info2->totalNumPcs;

  cct_node_t *cct_node = NULL;
  if (gpu_range_enabled()) {
    // Don't insert anything under kernel placeholder
    cct_node = gpu_range_context_cct_get(range_id, context_id);
  } else {
    // Can do safe insert under kernel placeholder
    cct_node = activity->cct_node;
  }

  // Translate a pc sample activity for each record
  pc_sampling_info2->translate(pc_sampling_data, total_num_pcs, period, range_id, cct_node);
  pc_sampling_info2->free(pc_sampling_data);

  TMSG(GPU_OPERATION, "PC sampling range %u\n", range_id);
}


static void
gpu_flush_process
(
 gpu_activity_t *activity
)
{
  // A special flush operation at the end of each thread
  // Set it false to indicate all previous activities have been processed
  if (atomic_load(activity->details.flush.wait)) {
    atomic_store(activity->details.flush.wait, false);
  }

  TMSG(GPU_OPERATION, "Flush activity\n");
}


static void
gpu_default_process
(
 gpu_activity_t *activity,
 gpu_activity_channel_t *channel
)
{
  gpu_activity_channel_produce(channel, activity);

  TMSG(GPU_OPERATION, "Default activity range %u\n", activity->range_id);
}

//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_operation_item_activity_process
(
 gpu_activity_t *activity,
 gpu_activity_channel_t *channel
)
{
  switch (activity->kind) {

    case GPU_ACTIVITY_MEMCPY:
      gpu_memcpy_process(activity, channel);
      break;

    case GPU_ACTIVITY_KERNEL:
      gpu_kernel_process(activity, channel);
      break;

    case GPU_ACTIVITY_SYNCHRONIZATION:
      gpu_synchronization_process(activity, channel);
      break;

    case GPU_ACTIVITY_FLUSH:
      gpu_flush_process(activity);
      break;

    case GPU_ACTIVITY_PC_SAMPLING_INFO2:
      gpu_pc_sampling_info2_process(activity);
      break;

    default:
      gpu_default_process(activity, channel);
      break;
  }
}


void
gpu_operation_item_process
(
 gpu_operation_item_t *it
)
{
  gpu_operation_item_activity_process(&(it->activity), it->channel);

  if (it->pending_operations) {
    atomic_fetch_add(it->pending_operations, -1);
  }
}

