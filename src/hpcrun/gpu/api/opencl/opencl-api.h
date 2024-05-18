// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _OPENCL_API_H_
#define _OPENCL_API_H_

//******************************************************************************
// local includes
//******************************************************************************

#include "../../activity/gpu-activity.h"
#include "../common/gpu-instrumentation.h"
#include "../../../../common/lean/hpcrun-opencl.h"
#include "opencl-memory-manager.h"



//************************ Forward Declarations ******************************

typedef struct opencl_object_t opencl_object_t;



//******************************************************************************
// interface operations
//******************************************************************************

cl_basic_callback_t
opencl_cb_basic_get
(
 opencl_object_t *
);


void
opencl_cb_basic_print
(
 cl_basic_callback_t,
 char *
);


void
opencl_initialize_correlation_id
(
 void
);


void
opencl_subscriber_callback
(
 opencl_object_t *
);


void
opencl_activity_completion_callback
(
 cl_event,
 cl_int,
 void *
);


void
opencl_timing_info_get
(
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  gpu_interval_t *,
  cl_event
);


cct_node_t *
opencl_api_node_get
(
 void
);


void
clSetEventCallback_wrapper
(
 cl_event,
 cl_int,
 void (CL_CALLBACK*)(cl_event, cl_int, void *),
 void *
);


void
opencl_api_initialize
(
 gpu_instrumentation_t *inst_options
);


uint64_t
get_numeric_hash_id_for_string
(
 const char *,
 size_t
);



void
opencl_optimization_check_enable
(
 void
);


void
opencl_blame_shifting_enable
(
 void
);


void
set_gpu_utilization_flag
(
 void
);


bool
get_gpu_utilization_flag
(
 void
);


cct_node_t*
place_cct_under_opencl_kernel
(
  uint32_t
);


void
opencl_api_thread_finalize
(
 void *,
 int
);


void
opencl_api_process_finalize
(
 void *,
 int
);


#endif  //_OPENCL_API_H_
