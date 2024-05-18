// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************




//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../cct/cct.h"

#include "../activity/gpu-activity.h"
#include "../activity/gpu-activity-channel.h"

#include "../trace/gpu-trace-api.h"
#include "../trace/gpu-trace-item.h"

#include <stdatomic.h>
#include "../../messages/messages.h"

#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"


//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "../common/gpu-print.h"



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
  gpu_trace_process_stream(device_id, context_id, stream_id, ti);
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

  if (activity->cct_node == NULL)
    hpcrun_terminate();

  gpu_trace_item_t entry_trace;

  gpu_trace_item_init(&entry_trace,
                      activity->details.memcpy.submit_time,
                      activity->details.memcpy.start,
                      activity->details.memcpy.end,
                      activity->cct_node);

  gpu_context_stream_trace(activity->details.memcpy.device_id,
                           activity->details.memcpy.context_id,
                           activity->details.memcpy.stream_id,
                           &entry_trace);

  gpu_activity_channel_send(channel, activity);

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

  gpu_trace_item_init(&entry_trace,
                      activity->details.kernel.submit_time,
                      activity->details.kernel.start,
                      activity->details.kernel.end,
                      activity->cct_node);

  gpu_context_stream_trace(activity->details.kernel.device_id,
                           activity->details.kernel.context_id,
                           activity->details.kernel.stream_id,
                           &entry_trace);

  gpu_activity_channel_send(channel, activity);

  PRINT("Kernel execution cct_node %p\n", activity->cct_node);
  PRINT("Kernel execution deviceId %u\n", activity->details.kernel.device_id);
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
gpu_unknown_process
(
 gpu_operation_item_t *it
)
{
  gpu_activity_t *activity = &it->activity;
  gpu_activity_channel_t *channel = it->channel;
  gpu_activity_channel_send(channel, activity);
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

    case GPU_ACTIVITY_FLUSH:
      gpu_flush_process(it);
      break;

    default:
      gpu_unknown_process(it);
      break;
  }

  if (it->pending_operations) {
    atomic_fetch_add(it->pending_operations, -1);
  }
}
