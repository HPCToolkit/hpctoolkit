// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef OPENCL_MEMORY_MANAGER_H
#define OPENCL_MEMORY_MANAGER_H



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../common/lean/bistack.h"
#include "../../activity/gpu-activity.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif


//************************ Forward Declarations ******************************

typedef struct cct_node_t cct_node_t;



//******************************************************************************
// type declarations
//******************************************************************************

//This must be the same as gpu_activity_kind_t
//typedef enum {
//  GPU_ACTIVITY_KERNEL                     = 1,
//  GPU_ACTIVITY_MEMCPY                     = 2
//} gpu_activity_kind_t;


typedef struct opencl_object_channel_t opencl_object_channel_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;

typedef struct cl_basic_callback_t {
  uint32_t correlation_id;
  gpu_activity_kind_t kind;
  gpu_memcpy_type_t type;
  cct_node_t *cct_node;
} cl_basic_callback_t;


typedef struct cl_kernel_callback_t {
  uint32_t correlation_id;
} cl_kernel_callback_t;


typedef struct cl_memcpy_callback_t {
  uint32_t correlation_id;
  gpu_memcpy_type_t type;
  bool fromHostToDevice;
  bool fromDeviceToHost;
  size_t size;
} cl_memcpy_callback_t;


typedef struct cl_memory_callback_t {
  uint32_t correlation_id;
  gpu_mem_type_t type;
  size_t size;
} cl_memory_callback_t;


typedef struct opencl_object_details_t {
  union {
    cl_kernel_callback_t ker_cb;
    cl_memcpy_callback_t cpy_cb;
    cl_memory_callback_t mem_cb;
  };
  gpu_activity_channel_t *initiator_channel;
  uint32_t context_id;
  uint32_t stream_id;
  uint32_t module_id;
  cct_node_t *cct_node;
  uint64_t submit_time;
} opencl_object_details_t;


typedef struct opencl_object_t {
  s_element_ptr_t next;
  opencl_object_channel_t *channel;
  gpu_activity_kind_t kind;
  bool internal_event;
  opencl_object_details_t details;
  atomic_int *pending_operations;
  void* pfn_clGetEventProfilingInfo;
  void* pfn_clReleaseEvent;
} opencl_object_t;



//******************************************************************************
// interface operations
//******************************************************************************

opencl_object_t *
opencl_malloc
(
 void
);


opencl_object_t *
opencl_malloc_kind
(
  void* pfn_clGetEventProfilingInfo,
  void* pfn_clReleaseEvent,
  gpu_activity_kind_t kind
);


void
opencl_free
(
 opencl_object_t *
);

#endif  //OPENCL_MEMORY_MANAGER_H
