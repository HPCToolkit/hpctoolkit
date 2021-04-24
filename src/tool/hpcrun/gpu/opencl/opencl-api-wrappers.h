#ifndef _OPENCL_API_WRAPPERS_H_
#define _OPENCL_API_WRAPPERS_H_

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/hpcrun-opencl.h>



//******************************************************************************
// interface operations
//******************************************************************************

cl_int
hpcrun_clBuildProgram
(
 cl_program program,
 cl_uint num_devices,
 const cl_device_id* device_list,
 const char* options,
 void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
 void* user_data
);


cl_context
hpcrun_clCreateContext
(
  const cl_context_properties *properties,
  cl_uint num_devices,
  const cl_device_id *devices,
  void (CL_CALLBACK* pfn_notify) (const char *errinfo, const void *private_info, size_t cb, void *user_data),
  void *user_data,
  cl_int *errcode_ret
);


cl_command_queue
hpcrun_clCreateCommandQueue
(
 cl_context context,
 cl_device_id device,
 cl_command_queue_properties properties,
 cl_int *errcode_ret
);


cl_command_queue
hpcrun_clCreateCommandQueueWithProperties
(
 cl_context context,
 cl_device_id device,
 const cl_queue_properties* properties,
 cl_int* errcode_ret
);


cl_int
hpcrun_clEnqueueNDRangeKernel
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
);


// this is a simplified version of clEnqueueNDRangeKernel, TODO: check if code duplication can be avoided
cl_int
hpcrun_clEnqueueTask
(
 cl_command_queue command_queue,
 cl_kernel kernel,
 cl_uint num_events_in_wait_list,
 const cl_event* event_wait_list,
 cl_event* event
);


cl_int
hpcrun_clEnqueueReadBuffer
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
);


cl_int
hpcrun_clEnqueueWriteBuffer
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
);


void*
hpcrun_clEnqueueMapBuffer
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
);


cl_mem
hpcrun_clCreateBuffer
(
 cl_context context,
 cl_mem_flags flags,
 size_t size,
 void* host_ptr,
 cl_int* errcode_ret
);


cl_int
hpcrun_clWaitForEvents
(
	cl_uint num_events,
	const cl_event* event_list
);


cl_int
hpcrun_clFinish
(
 cl_command_queue command_queue
);


cl_int
hpcrun_clSetKernelArg
(
 cl_kernel kernel,
 cl_uint arg_index,
 size_t arg_size,
 const void* arg_value
);


cl_int
hpcrun_clReleaseMemObject
(
 cl_mem mem
);


cl_int
hpcrun_clReleaseKernel
(
 cl_kernel kernel
);


cl_int
hpcrun_clReleaseCommandQueue
(
 cl_command_queue command_queue
);

#endif  // _OPENCL_API_WRAPPERS_H_
