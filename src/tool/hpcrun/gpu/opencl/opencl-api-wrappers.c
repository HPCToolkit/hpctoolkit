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
// Copyright ((c)) 2002-2021, Rice University
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
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>

//******************************************************************************
// OpenCL public API override
//******************************************************************************


//#ifdef ENABLE_GTPIN
#if 1
// one downside of this appproach is that we may override the callback provided by user
cl_int
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
  return hpcrun_clBuildProgram(program, num_devices, device_list,
			options, pfn_notify,
			user_data);
}
#endif // ENABLE_GTPIN


cl_context
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
  return hpcrun_clCreateContext(properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
}


cl_command_queue
clCreateCommandQueue
(
 cl_context context,
 cl_device_id device,
 cl_command_queue_properties properties,
 cl_int *errcode_ret
)
{
  return hpcrun_clCreateCommandQueue(context, device,
        properties,errcode_ret);
}

cl_command_queue
clCreateCommandQueueWithProperties
(
 cl_context context,
 cl_device_id device,
 const cl_queue_properties* properties,
 cl_int* errcode_ret
)
{
  return hpcrun_clCreateCommandQueueWithProperties(
			      context, device, properties, errcode_ret);
}


cl_int
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
  return hpcrun_clEnqueueNDRangeKernel(
       command_queue, ocl_kernel, work_dim, global_work_offset,
	global_work_size, local_work_size, num_events_in_wait_list,
	event_wait_list, event);
}

cl_int
clEnqueueTask
(
 cl_command_queue command_queue,
 cl_kernel kernel,
 cl_uint num_events_in_wait_list,
 const cl_event* event_wait_list,
 cl_event* event
)
{
  return hpcrun_clEnqueueTask(
      command_queue, kernel, num_events_in_wait_list,
      event_wait_list, event
  );
}


cl_int
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
  return hpcrun_clEnqueueReadBuffer(
      command_queue, buffer, blocking_read, offset,
	  cb, ptr, num_events_in_wait_list,
	  event_wait_list, event);
}

cl_int
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
  return hpcrun_clEnqueueWriteBuffer(
      command_queue, buffer, blocking_write, offset, cb,
      ptr, num_events_in_wait_list, event_wait_list, event
  );
}


void*
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
  return hpcrun_clEnqueueMapBuffer(
      command_queue, buffer, blocking_map, map_flags,
      offset, size, num_events_in_wait_list,
      event_wait_list, event, errcode_ret
  );
}


cl_mem
clCreateBuffer
(
 cl_context context,
 cl_mem_flags flags,
 size_t size,
 void* host_ptr,
 cl_int* errcode_ret
)
{
  return hpcrun_clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
}


// comment if opencl blame-shifting is disabled
cl_int
clWaitForEvents
(
	cl_uint num_events,
	const cl_event* event_list
)
{
	return hpcrun_clWaitForEvents(num_events, event_list);
}


cl_int
clSetKernelArg
(
 cl_kernel kernel,
 cl_uint arg_index,
 size_t arg_size,
 const void* arg_value
)
{
  return hpcrun_clSetKernelArg(kernel, arg_index, arg_size, arg_value);
}


cl_int
clReleaseMemObject
(
 cl_mem mem
)
{
  return hpcrun_clReleaseMemObject(mem);
}


cl_int
clFinish
(
	cl_command_queue command_queue
)
{
	return hpcrun_clFinish(command_queue);
}


cl_int
clReleaseKernel
(
 cl_kernel kernel
)
{
  return hpcrun_clReleaseKernel(kernel);
}


cl_int
clReleaseCommandQueue
(
 cl_command_queue command_queue
)
{
  return hpcrun_clReleaseCommandQueue(command_queue);
}

