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

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#include <gotcha/gotcha.h>
#include <inttypes.h>
#include <stdio.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/messages/messages.h>
#include <lib/prof-lean/hpcrun-gotcha.h>
#include <lib/prof-lean/stdatomic.h>

#include "opencl-api.h"
#include "opencl-intercept.h"
#include "opencl-memory-manager.h"



//******************************************************************************
// local data
//******************************************************************************

static gotcha_wrappee_handle_t clCreateCommandQueue_handle;
static gotcha_wrappee_handle_t clEnqueueNDRangeKernel_handle;
static gotcha_wrappee_handle_t clEnqueueReadBuffer_handle;
static gotcha_wrappee_handle_t clEnqueueWriteBuffer_handle;
static atomic_long correlation_id;



//******************************************************************************
// private operations
//******************************************************************************

static uint64_t
getCorrelationId
(
  void
)
{
  return atomic_fetch_add(&correlation_id, 1);
}


static void
initializeKernelCallBackInfo
(
  cl_kernel_callback_t* kernel_cb,
  uint64_t correlation_id
)
{
  kernel_cb->correlation_id = correlation_id;
  kernel_cb->type = kernel; 
}


static void
initializeMemoryCallBackInfo
(
  cl_memory_callback_t* mem_transfer_cb,
  uint64_t correlation_id,
  size_t size,
  bool fromHostToDevice
)
{
  mem_transfer_cb->correlation_id = correlation_id;
  mem_transfer_cb->type = (fromHostToDevice) ? memcpy_H2D: memcpy_D2H; 
  mem_transfer_cb->size = size;
  mem_transfer_cb->fromHostToDevice = fromHostToDevice;
  mem_transfer_cb->fromDeviceToHost = !fromHostToDevice;
}


static cl_command_queue
clCreateCommandQueue_wrapper
(
  cl_context context,
  cl_device_id device,
  cl_command_queue_properties properties,
  cl_int *errcode_ret
)
{
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; // enabling profiling
  clqueue_t clCreateCommandQueue_wrappee = GOTCHA_GET_TYPED_WRAPPEE(clCreateCommandQueue_handle, clqueue_t);
  return clCreateCommandQueue_wrappee(context, device, properties, errcode_ret);
}


static cl_int
clEnqueueNDRangeKernel_wrapper
(
  cl_command_queue command_queue,
  cl_kernel ocl_kernel,
  cl_uint work_dim,
  const size_t *global_work_offset, 
  const size_t *global_work_size,
  const size_t *local_work_size,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  uint64_t correlation_id = getCorrelationId();
  opencl_object_t *kernel_info = opencl_malloc();
  kernel_info->kind = OPENCL_KERNEL_CALLBACK;
  cl_kernel_callback_t *kernel_cb = &(kernel_info->details.ker_cb);
  initializeKernelCallBackInfo(kernel_cb, correlation_id);
  opencl_object_t* e;
  if(!event) {
    e = opencl_malloc();
    event = &(e->details.event);
  }
  clkernel_t clEnqueueNDRangeKernel_wrappee = GOTCHA_GET_TYPED_WRAPPEE(clEnqueueNDRangeKernel_handle, clkernel_t);
  cl_int return_status = clEnqueueNDRangeKernel_wrappee(command_queue, ocl_kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
  ETMSG(CL, "registering callback for type: kernel. Correlation id: %"PRIu64 "", correlation_id);
  opencl_subscriber_callback(kernel_cb->type, kernel_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, kernel_info);
  //opencl_free(e);
  return return_status;
}


static cl_int
clEnqueueReadBuffer_wrapper
(
  cl_command_queue command_queue,
  cl_mem buffer,
  cl_bool blocking_read,
  size_t offset,
  size_t cb,
  void *ptr,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  uint64_t correlation_id = getCorrelationId();
  opencl_object_t *mem_info = opencl_malloc();
  mem_info->kind = OPENCL_MEMORY_CALLBACK;
  cl_memory_callback_t *mem_transfer_cb = &(mem_info->details.mem_cb);
  initializeMemoryCallBackInfo(mem_transfer_cb, correlation_id, cb, false);
  opencl_object_t* e;
  if(!event) {
    e = opencl_malloc();
    event = &(e->details.event);
  }
  clreadbuffer_t clEnqueueReadBuffer_wrappee = GOTCHA_GET_TYPED_WRAPPEE(clEnqueueReadBuffer_handle, clreadbuffer_t);
  cl_int return_status = clEnqueueReadBuffer_wrappee(command_queue, buffer, blocking_read, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
  ETMSG(CL, "registering callback for type: D2H. Correlation id: %"PRIu64 "", correlation_id);
  ETMSG(CL, "%d(bytes) of data being transferred from device to host", (long)cb);
  opencl_subscriber_callback(mem_transfer_cb->type, mem_transfer_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, mem_info);
  //opencl_free(e);
  return return_status;
}


static cl_int
clEnqueueWriteBuffer_wrapper
(
  cl_command_queue command_queue,
  cl_mem buffer,
  cl_bool blocking_write,
  size_t offset,
  size_t cb,
  const void *ptr,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  uint64_t correlation_id = getCorrelationId();
  opencl_object_t *mem_info = opencl_malloc();
  mem_info->kind = OPENCL_MEMORY_CALLBACK;
  cl_memory_callback_t *mem_transfer_cb = &(mem_info->details.mem_cb);
  initializeMemoryCallBackInfo(mem_transfer_cb, correlation_id, cb, true);
  opencl_object_t* e;
  if(!event) {
    e = opencl_malloc();
    event = &(e->details.event);
  }
  clwritebuffer_t clEnqueueWriteBuffer_wrappee = GOTCHA_GET_TYPED_WRAPPEE(clEnqueueWriteBuffer_handle, clwritebuffer_t);
  cl_int return_status = clEnqueueWriteBuffer_wrappee(command_queue, buffer, blocking_write, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
  ETMSG(CL, "registering callback for type: H2D. Correlation id: %"PRIu64 "", correlation_id);
  ETMSG(CL, "%d(bytes) of data being transferred from host to device", (long)cb);
  opencl_subscriber_callback(mem_transfer_cb->type, mem_transfer_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, mem_info);
  //opencl_free(e);
  return return_status;
}



//******************************************************************************
// gotcha variables
//******************************************************************************

static gotcha_binding_t queue_wrapper[] = {
  {
    "clCreateCommandQueue",
    (void*) clCreateCommandQueue_wrapper,
    &clCreateCommandQueue_handle}
};

static gotcha_binding_t kernel_wrapper[] = {
  {
    "clEnqueueNDRangeKernel",
    (void*)clEnqueueNDRangeKernel_wrapper,
    &clEnqueueNDRangeKernel_handle
  }
};

static gotcha_binding_t buffer_wrapper[] = {
  {
    "clEnqueueReadBuffer",
    (void*) clEnqueueReadBuffer_wrapper,
    &clEnqueueReadBuffer_handle
  },
  {
    "clEnqueueWriteBuffer",
    (void*) clEnqueueWriteBuffer_wrapper,
    &clEnqueueWriteBuffer_handle
  }
};



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_intercept_initialize
(
  void
)
{
  atomic_store(&correlation_id, 0);
}


void
opencl_setup_intercept
(
  void
)
{
  gotcha_wrap(queue_wrapper, 1, "queue_intercept");
  gotcha_wrap(kernel_wrapper, 1, "kernel_intercept");
  gotcha_wrap(buffer_wrapper, 2, "memory_intercept");
}


void
opencl_teardown_intercept
(
  void
)
{
  // not sure if this works
  gotcha_set_priority("queue_intercept", -1);
  gotcha_set_priority("kernel_intercept", -1);
  gotcha_set_priority("memory_intercept", -1);
}