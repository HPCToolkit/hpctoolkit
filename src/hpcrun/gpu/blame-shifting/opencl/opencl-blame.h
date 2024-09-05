// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef blame_shift_opencl_h
#define blame_shift_opencl_h

//******************************************************************************
// local includes
//******************************************************************************

#include <CL/cl.h>



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_queue_prologue
(
 cl_command_queue queue
);


void
opencl_queue_epilogue
(
 cl_command_queue queue
);


void
opencl_kernel_prologue
(
  typeof(&clRetainEvent) pfn_clRetainEvent,
  cl_event event,
  uint32_t kernel_module_id
);


void
opencl_kernel_epilogue
(
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  cl_event event
);


void
opencl_sync_prologue
(
 cl_command_queue queue
);


void
opencl_sync_epilogue
(
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  cl_command_queue queue,
  uint16_t num_sync_events
);

#endif  //blame_shift_opencl_h
