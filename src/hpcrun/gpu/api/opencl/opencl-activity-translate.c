// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <assert.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../activity/gpu-activity.h"
#include "../../../messages/messages.h"

#include "opencl-activity-translate.h"
#include "opencl-api.h"



//******************************************************************************
// private operations
//******************************************************************************

static void
convert_kernel_launch
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  uint64_t start_time,
  uint64_t end_time
)
{
  memset(&ga->details.kernel, 0, sizeof(gpu_kernel_t));
  if (start_time != 0 && end_time != 0) {
    gpu_interval_set(&ga->details.interval, start_time, end_time);
  }

  ga->kind     = cb_data->kind;
  ga->cct_node = cb_data->details.cct_node;

  ga->details.kernel.correlation_id = cb_data->details.ker_cb.correlation_id;
  ga->details.kernel.submit_time    = cb_data->details.submit_time;
  ga->details.kernel.context_id    = cb_data->details.context_id;
  ga->details.kernel.stream_id    = cb_data->details.stream_id;
}


static void
convert_memcpy
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  uint64_t start_time,
  uint64_t end_time
)
{
  memset(&ga->details.memcpy, 0, sizeof(gpu_memcpy_t));
  if (start_time != 0 && end_time != 0) {
    gpu_interval_set(&ga->details.interval, start_time, end_time);
  }

  ga->kind     = cb_data->kind;
  ga->cct_node = cb_data->details.cct_node;

  ga->details.memcpy.correlation_id  = cb_data->details.cpy_cb.correlation_id;
  ga->details.memcpy.submit_time     = cb_data->details.submit_time;
  ga->details.memcpy.context_id      = cb_data->details.context_id;
  ga->details.memcpy.stream_id       = cb_data->details.stream_id;
  ga->details.memcpy.bytes           = cb_data->details.cpy_cb.size;
  ga->details.memcpy.copyKind        = cb_data->details.cpy_cb.type;
}


static void
convert_memory
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  uint64_t start_time,
  uint64_t end_time
)
{
  memset(&ga->details.memory, 0, sizeof(gpu_memory_t));
  if (start_time != 0 && end_time != 0) {
    gpu_interval_set(&ga->details.interval, start_time, end_time);
  }

  ga->kind     = cb_data->kind;
  ga->cct_node = cb_data->details.cct_node;

  ga->details.memory.correlation_id  = cb_data->details.mem_cb.correlation_id;
  ga->details.memory.bytes           = cb_data->details.mem_cb.size;
  ga->details.memory.memKind         = cb_data->details.mem_cb.type;
}


//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_activity_translate
(
  gpu_activity_t *ga,
  opencl_object_t *cb_data,
  gpu_interval_t interval
)
{
  switch (cb_data->kind) {
    case GPU_ACTIVITY_MEMCPY:
      convert_memcpy(ga, cb_data, interval.start, interval.end);
      break;

    case GPU_ACTIVITY_KERNEL:
      convert_kernel_launch(ga, cb_data, interval.start, interval.end);
      break;

    case GPU_ACTIVITY_MEMORY:
      convert_memory(ga, cb_data, interval.start, interval.end);
      break;

    default:
      assert(false && "Invalid activity kind!");
      hpcrun_terminate();
  }

  cstack_ptr_set(&(ga->next), 0);
}
