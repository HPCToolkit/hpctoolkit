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


static void
trace_item_set
(
 gpu_trace_item_t *ti,
 uint64_t submit_time,
 uint64_t start_time,
 uint64_t end_time,
 gpu_activity_t *ga
)
{
  gpu_trace_item_produce(ti, submit_time, start_time, end_time, ga->cct_node);
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
  trace_item_set(&entry_trace, activity->details.memcpy.submit_time,
                 activity->details.memcpy.start, activity->details.memcpy.end, activity);

  gpu_context_stream_trace
    (activity->details.memcpy.context_id, activity->details.memcpy.stream_id,
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
  trace_item_set(&entry_trace, activity->details.kernel.submit_time,
                 activity->details.kernel.start, activity->details.kernel.end, activity);

  gpu_context_stream_trace
    (activity->details.kernel.context_id, activity->details.kernel.stream_id,
     &entry_trace);

  gpu_activity_channel_produce(channel, activity);

  PRINT("Kernel execution cct_node %p\n", activity->cct_node);
  PRINT("Kernel execution deviceId %u\n", activity->details.kernel.device_id);
}


static void
gpu_unknown_process
(
gpu_operation_item_t *it
)
{
  PRINT("Unknown activity kind %d\n", it->activity->kind);
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

  default:
    gpu_unknown_process(it);
    break;
  }
}

