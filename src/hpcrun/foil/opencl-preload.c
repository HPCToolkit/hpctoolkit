// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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

#define _GNU_SOURCE

#include "foil.h"
#include "../gpu/api/opencl/opencl-api-wrappers.h"
#include "../../common/lean/hpcrun-opencl.h"


FOIL_DLSYM_FUNC(clGetKernelInfo);
FOIL_DLSYM_FUNC(clRetainEvent);
FOIL_DLSYM_FUNC(clReleaseEvent);
FOIL_DLSYM_FUNC(clGetEventInfo);
FOIL_DLSYM_FUNC(clGetEventProfilingInfo);
FOIL_DLSYM_FUNC(clGetPlatformIDs);
FOIL_DLSYM_FUNC(clGetDeviceIDs);
FOIL_DLSYM_FUNC(clGetProgramInfo);
FOIL_DLSYM_FUNC(clSetEventCallback);


HPCRUN_EXPOSED cl_int
clBuildProgram
(
 cl_program program,
 cl_uint num_devices,
 const cl_device_id* device_list,
 const char* options,
 void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
 void* user_data
)
{
  LOOKUP_FOIL_BASE(base, clBuildProgram);
  FOIL_DLSYM(real, clBuildProgram);
  return base(real, pfn_clGetProgramInfo(), program, num_devices, device_list,
                        options, pfn_notify,
                        user_data);
}


HPCRUN_EXPOSED cl_context
clCreateContext
(
 const cl_context_properties *properties,
 cl_uint num_devices,
 const cl_device_id *devices,
 void (CL_CALLBACK* pfn_notify) (const char *errinfo, const void *private_info, size_t cb, void *user_data),
 void *user_data,
 cl_int *errcode_ret
)
{
  LOOKUP_FOIL_BASE(base, clCreateContext);
  FOIL_DLSYM(real, clCreateContext);
  return base(real, pfn_clGetPlatformIDs(), pfn_clGetDeviceIDs(),
    properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
}


HPCRUN_EXPOSED cl_command_queue
clCreateCommandQueue
(
 cl_context context,
 cl_device_id device,
 cl_command_queue_properties properties,
 cl_int *errcode_ret
)
{
  LOOKUP_FOIL_BASE(base, clCreateCommandQueue);
  FOIL_DLSYM(real, clCreateCommandQueue);
  return base(real, context, device,
        properties,errcode_ret);
}

HPCRUN_EXPOSED cl_command_queue
clCreateCommandQueueWithProperties
(
 cl_context context,
 cl_device_id device,
 const cl_queue_properties* properties,
 cl_int* errcode_ret
)
{
  LOOKUP_FOIL_BASE(base, clCreateCommandQueueWithProperties);
  FOIL_DLSYM(real, clCreateCommandQueueWithProperties);
  return base(real, context, device, properties, errcode_ret);
}


HPCRUN_EXPOSED cl_int
clEnqueueNDRangeKernel
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
  LOOKUP_FOIL_BASE(base, clEnqueueNDRangeKernel);
  FOIL_DLSYM(real, clEnqueueNDRangeKernel);
  return base(real, pfn_clGetKernelInfo(), pfn_clRetainEvent(), pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), pfn_clSetEventCallback(),
       command_queue, ocl_kernel, work_dim, global_work_offset,
        global_work_size, local_work_size, num_events_in_wait_list,
        event_wait_list, event);
}

HPCRUN_EXPOSED cl_int
clEnqueueTask
(
 cl_command_queue command_queue,
 cl_kernel kernel,
 cl_uint num_events_in_wait_list,
 const cl_event* event_wait_list,
 cl_event* event
)
{
  LOOKUP_FOIL_BASE(base, clEnqueueTask);
  FOIL_DLSYM(real, clEnqueueTask);
  return base(real, pfn_clGetKernelInfo(), pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), pfn_clSetEventCallback(),
      command_queue, kernel, num_events_in_wait_list,
      event_wait_list, event
  );
}


HPCRUN_EXPOSED cl_int
clEnqueueReadBuffer
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
  LOOKUP_FOIL_BASE(base, clEnqueueReadBuffer);
  FOIL_DLSYM(real, clEnqueueReadBuffer);
  return base(real, pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), pfn_clSetEventCallback(),
      command_queue, buffer, blocking_read, offset,
          cb, ptr, num_events_in_wait_list,
          event_wait_list, event);
}

HPCRUN_EXPOSED cl_int
clEnqueueWriteBuffer
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
  LOOKUP_FOIL_BASE(base, clEnqueueWriteBuffer);
  FOIL_DLSYM(real, clEnqueueWriteBuffer);
  return base(real, pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), pfn_clSetEventCallback(),
      command_queue, buffer, blocking_write, offset, cb,
      ptr, num_events_in_wait_list, event_wait_list, event
  );
}


HPCRUN_EXPOSED void*
clEnqueueMapBuffer
(
 cl_command_queue command_queue,
 cl_mem buffer,
 cl_bool blocking_map,
 cl_map_flags map_flags,
 size_t offset,
 size_t size,
 cl_uint num_events_in_wait_list,
 const cl_event* event_wait_list,
 cl_event* event,
 cl_int* errcode_ret
)
{
  LOOKUP_FOIL_BASE(base, clEnqueueMapBuffer);
  FOIL_DLSYM(real, clEnqueueMapBuffer);
  return base(real, pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), pfn_clSetEventCallback(),
      command_queue, buffer, blocking_map, map_flags,
      offset, size, num_events_in_wait_list,
      event_wait_list, event, errcode_ret
  );
}


HPCRUN_EXPOSED cl_mem
clCreateBuffer
(
 cl_context context,
 cl_mem_flags flags,
 size_t size,
 void* host_ptr,
 cl_int* errcode_ret
)
{
  LOOKUP_FOIL_BASE(base, clCreateBuffer);
  FOIL_DLSYM(real, clCreateBuffer);
  return base(real, pfn_clGetEventProfilingInfo(), pfn_clReleaseEvent(), context, flags, size, host_ptr, errcode_ret);
}


HPCRUN_EXPOSED cl_int
clSetKernelArg
(
 cl_kernel kernel,
 cl_uint arg_index,
 size_t arg_size,
 const void* arg_value
)
{
  LOOKUP_FOIL_BASE(base, clSetKernelArg);
  FOIL_DLSYM(real, clSetKernelArg);
  return base(real, kernel, arg_index, arg_size, arg_value);
}


HPCRUN_EXPOSED cl_int
clReleaseMemObject
(
 cl_mem mem
)
{
  LOOKUP_FOIL_BASE(base, clReleaseMemObject);
  FOIL_DLSYM(real, clReleaseMemObject);
  return base(real, mem);
}


HPCRUN_EXPOSED cl_int
clReleaseKernel
(
 cl_kernel kernel
)
{
  LOOKUP_FOIL_BASE(base, clReleaseKernel);
  FOIL_DLSYM(real, clReleaseKernel);
  return base(real, kernel);
}


// comment if opencl blame-shifting is disabled
HPCRUN_EXPOSED cl_int
clWaitForEvents
(
        cl_uint num_events,
        const cl_event* event_list
)
{
        LOOKUP_FOIL_BASE(base, clWaitForEvents);
        FOIL_DLSYM(real, clWaitForEvents);
        return base(real, pfn_clReleaseEvent(), pfn_clGetEventInfo(), num_events, event_list);
}


HPCRUN_EXPOSED cl_int
clReleaseCommandQueue
(
 cl_command_queue command_queue
)
{
  LOOKUP_FOIL_BASE(base, clReleaseCommandQueue);
  FOIL_DLSYM(real, clReleaseCommandQueue);
  return base(real, command_queue);
}


HPCRUN_EXPOSED cl_int
clFinish
(
        cl_command_queue command_queue
)
{
        LOOKUP_FOIL_BASE(base, clFinish);
        FOIL_DLSYM(real, clFinish);
        return base(real, pfn_clReleaseEvent(), command_queue);
}
