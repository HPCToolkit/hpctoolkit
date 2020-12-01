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

#include <inttypes.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/messages/messages.h>
#include <lib/prof-lean/hpcrun-gotcha.h>
#include <lib/prof-lean/hpcrun-opencl.h>
#include <lib/prof-lean/stdatomic.h>

#include "opencl-api.h"
#include "opencl-intercept.h"
#include "opencl-memory-manager.h"



//******************************************************************************
// local data
//******************************************************************************

#ifndef HPCRUN_STATIC_LINK
static gotcha_wrappee_handle_t clCreateCommandQueue_handle;
static gotcha_wrappee_handle_t clEnqueueNDRangeKernel_handle;
static gotcha_wrappee_handle_t clEnqueueReadBuffer_handle;
static gotcha_wrappee_handle_t clEnqueueWriteBuffer_handle;
static atomic_long correlation_id;



//******************************************************************************
// private operations
//******************************************************************************

static void
opencl_intercept_initialize
(
  void
)
{
  atomic_store(&correlation_id, 0);
}


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
  cl_kernel_callback_t *kernel_cb,
  uint64_t correlation_id
)
{
  kernel_cb->correlation_id = correlation_id;
  kernel_cb->type = kernel; 
}


static void
initializeMemoryCallBackInfo
(
  cl_memory_callback_t *mem_transfer_cb,
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
  // enabling profiling
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; 

  clqueue_t clCreateCommandQueue_wrappee = 
    GOTCHA_GET_TYPED_WRAPPEE(clCreateCommandQueue_handle, clqueue_t);

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
  cl_event my_event;
  cl_event *eventp;
  if (!event) {
    kernel_info->isInternalClEvent = true;
    eventp = &my_event;
  } else {
    eventp = event;
    kernel_info->isInternalClEvent = false;
  }
  clkernel_t clEnqueueNDRangeKernel_wrappee = 
    GOTCHA_GET_TYPED_WRAPPEE(clEnqueueNDRangeKernel_handle, clkernel_t);
  cl_int return_status = 
    clEnqueueNDRangeKernel_wrappee(command_queue, ocl_kernel, work_dim, 
				   global_work_offset, global_work_size, 
				   local_work_size, num_events_in_wait_list, 
				   event_wait_list, eventp);

  ETMSG(OPENCL, "registering callback for type: kernel. " 
	"Correlation id: %"PRIu64 "", correlation_id);

  opencl_subscriber_callback(kernel_cb->type, kernel_cb->correlation_id);
  clSetEventCallback_wrapper(*eventp, CL_COMPLETE, 
			     &opencl_activity_completion_callback, kernel_info);
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
  cl_event my_event;
  cl_event *eventp;
  if (!event) {
    mem_info->isInternalClEvent = true;
    eventp = &my_event;
  } else {
    eventp = event;
    mem_info->isInternalClEvent = false;
  }
  clreadbuffer_t clEnqueueReadBuffer_wrappee = 
    GOTCHA_GET_TYPED_WRAPPEE(clEnqueueReadBuffer_handle, clreadbuffer_t);
  cl_int return_status = 
    clEnqueueReadBuffer_wrappee(command_queue, buffer, blocking_read, offset, 
				cb, ptr, num_events_in_wait_list, 
				event_wait_list, eventp);

  ETMSG(OPENCL, "registering callback for type: D2H. " 
	"Correlation id: %"PRIu64 "", correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host", 
	(long)cb);

  opencl_subscriber_callback(mem_transfer_cb->type, 
			     mem_transfer_cb->correlation_id);

  clSetEventCallback_wrapper(*eventp, CL_COMPLETE, 
			     &opencl_activity_completion_callback, mem_info);

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
  cl_event my_event;
  cl_event *eventp;
  if (!event) {
    mem_info->isInternalClEvent = true;
    eventp = &my_event;
  } else {
    eventp = event;
    mem_info->isInternalClEvent = false;
  }
  clwritebuffer_t clEnqueueWriteBuffer_wrappee = 
    GOTCHA_GET_TYPED_WRAPPEE(clEnqueueWriteBuffer_handle, clwritebuffer_t);
  cl_int return_status = 
    clEnqueueWriteBuffer_wrappee(command_queue, buffer, blocking_write, offset,
				 cb, ptr, num_events_in_wait_list, 
				 event_wait_list, eventp);

  ETMSG(OPENCL, "registering callback for type: H2D. " 
	"Correlation id: %"PRIu64 "", correlation_id);

  ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device", 
	(long)cb);

  opencl_subscriber_callback(mem_transfer_cb->type, 
			     mem_transfer_cb->correlation_id);

  clSetEventCallback_wrapper(*eventp, CL_COMPLETE, 
			     &opencl_activity_completion_callback, 
			     (void*) mem_info);

  return return_status;
}

#endif



//******************************************************************************
// gotcha variables
//******************************************************************************

#ifndef HPCRUN_STATIC_LINK
static gotcha_binding_t opencl_bindings[] = {
  {
    "clCreateCommandQueue",
    (void*) clCreateCommandQueue_wrapper,
    &clCreateCommandQueue_handle
  },
  {
    "clEnqueueNDRangeKernel",
    (void*)clEnqueueNDRangeKernel_wrapper,
    &clEnqueueNDRangeKernel_handle
  },
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
#endif



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_intercept_setup
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK
  ETMSG(OPENCL, "setting up opencl intercepts");
  gotcha_wrap(opencl_bindings, 4, "opencl_bindings");
  opencl_intercept_initialize();
#endif
}


void
opencl_intercept_teardown
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK
  // not sure if this works
  gotcha_set_priority("opencl_bindings", -1);
#endif
}
