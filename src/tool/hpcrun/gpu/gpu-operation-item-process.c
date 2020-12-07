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
// Copyright ((c)) 2002-2020, Rice University
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
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-trace-item.h>
#include <hpcrun/gpu/gpu-context-id-map.h>
#include <lib/prof-lean/stdatomic.h>

#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"


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
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_item_t *ti
)
{
  gpu_context_id_map_stream_process(context_id, stream_id, gpu_trace_produce, ti);
}



//******************************************************************************
// gpu operations process
//******************************************************************************

static void
gpu_memcpy_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_activity_channel_t *channel = it->channel;

  assert(activity->cct_node != NULL);

  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.memcpy.submit_time,
                         activity->details.memcpy.start,
                         activity->details.memcpy.end,
                         activity->cct_node);

  gpu_context_stream_trace(activity->details.memcpy.context_id,
                           activity->details.memcpy.stream_id,
                           &entry_trace);

  gpu_activity_channel_produce(channel, activity);

  PRINT("Memcpy copy cct_node %p\n", activity->cct_node);
  PRINT("Memcpy copy kind %u\n", activity->details.memcpy.copyKind);
  PRINT("Memcpy copy bytes %lu\n", activity->details.memcpy.bytes);
}


static void
gpu_kernel_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_activity_channel_t *channel = it->channel;

  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.kernel.submit_time,
                         activity->details.kernel.start,
                         activity->details.kernel.end,
                         activity->cct_node);

  gpu_context_stream_trace(activity->details.kernel.context_id,
                           activity->details.kernel.stream_id,
                           &entry_trace);

  gpu_activity_channel_produce(channel, activity);

  PRINT("Kernel execution cct_node %p\n", activity->cct_node);
  PRINT("Kernel execution deviceId %u\n", activity->details.kernel.device_id);
}


static void
gpu_synchronization_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_activity_channel_t *channel = it->channel;

  gpu_trace_item_t entry_trace;

  gpu_trace_item_produce(&entry_trace,
                         activity->details.synchronization.submit_time,
                         activity->details.synchronization.start,
                         activity->details.synchronization.end,
                         activity->cct_node);

  if (activity->kind == GPU_ACTIVITY_SYNCHRONIZATION) {
    uint32_t context_id = activity->details.synchronization.context_id;
    uint32_t stream_id = activity->details.synchronization.stream_id;

    switch (activity->details.synchronization.syncKind) {
      case GPU_SYNC_STREAM:
      case GPU_SYNC_STREAM_EVENT_WAIT:
        // Insert a event for a specific stream
        PRINT("Add context %u stream %u sync\n", context_id, stream_id);
        gpu_context_id_map_stream_process(context_id, stream_id, gpu_trace_produce, &entry_trace);
        break;
      case GPU_SYNC_CONTEXT:
        // Insert events for all current active streams
        // TODO(Keren): What if the stream is created
        PRINT("Add context %u sync\n", context_id);
        gpu_context_id_map_context_process(context_id, gpu_trace_produce, &entry_trace);
        break;
      case GPU_SYNC_EVENT:
        PRINT("Add context %u stream %u sync\n", context_id, stream_id);
        gpu_context_id_map_stream_process(context_id, stream_id, gpu_trace_produce, &entry_trace);
        break;
      default:
        // invalid
        PRINT("Invalid synchronization %u\n", correlation_id);
    }
  }

  gpu_activity_channel_produce(channel, activity);
}


static void
gpu_range_kernel_process
(
 gpu_operation_item_t *it
)
{
}


static void
gpu_pc_sampling_info2_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_pc_sampling_info2_t pc_sampling_info2 = activity->details.pc_sampling_info2;

  void *pc_sampling_data = pc_sampling_info2.pcSamplingData;
  uint32_t period = pc_sampling_info2.samplingPeriodInCycles;
  uint64_t total_num_pcs = pc_sampling_info2.totalNumPcs;

  static gpu_pc_sampling2_t gpu_pc_sampling[GPU_INST_STALL2_INVALID];

  // 1. translate a pc sample activity for each record
  gpu_activity_channel_t *channel;
  for (uint64_t index = 0; index < total_num_pcs; ++index) {
    pc_sampling_info2.translate(pc_sampling_data, index, gpu_pc_sampling, period);

    for (size_t i = 0; i < GPU_INST_STALL2_INVALID; ++i) {
      if (gpu_pc_sampling[i].samples != 0) {
        //channel = gpu_range_profiling_pc_sampling_process(&gpu_pc_sampling[i]);
        //gpu_activity_channel_produce(channel, &gpu_pc_sampling[i]);
      }
    }
  }

  // 2. produce overall metrics
  //gpu_trace_metrics_t 
  //gpu_range_pc_sampling_overall(range_id);

  // 3. clear the current range 
  //gpu_range_id_map_delete(range_id);
}


static void
gpu_flush_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  // A special flush operation at the end of each thread
  // Set it false to indicate all previous activities have been processed
  if (atomic_load(activity->details.flush.wait)) {
    atomic_store(activity->details.flush.wait, false);
  }
}


static void
gpu_default_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_activity_channel_t *channel = it->channel;
  gpu_activity_channel_produce(channel, activity);
}

//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_operation_item_process
(
 gpu_operation_item_t *it
)
{
  switch (it->activity.kind) {

    case GPU_ACTIVITY_MEMCPY:
      gpu_memcpy_process(it);
      break;

    case GPU_ACTIVITY_KERNEL:
      gpu_kernel_process(it);
      break;

    case GPU_ACTIVITY_SYNCHRONIZATION:
      gpu_synchronization_process(it);
      break;

    case GPU_ACTIVITY_FLUSH:
      gpu_flush_process(it);
      break;

    case GPU_ACTIVITY_PC_SAMPLING_INFO2:
      gpu_pc_sampling_info2_process(it);
      break;

    default:
      gpu_default_process(it);
      break;
  }

  if (it->pending_operations) {
    atomic_fetch_add(it->pending_operations, -1);
  }
}

