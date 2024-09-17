// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "opencl.h"

#include "../gpu/api/opencl/opencl-api-wrappers.h"
#include "common.h"
#include "opencl-private.h"

const struct hpcrun_foil_hookdispatch_opencl* hpcrun_foil_fetch_hooks_opencl() {
  static const struct hpcrun_foil_hookdispatch_opencl hooks = {
      .clBuildProgram = hpcrun_clBuildProgram,
      .clCreateContext = hpcrun_clCreateContext,
      .clCreateCommandQueue = hpcrun_clCreateCommandQueue,
      .clCreateCommandQueueWithProperties = hpcrun_clCreateCommandQueueWithProperties,
      .clEnqueueNDRangeKernel = hpcrun_clEnqueueNDRangeKernel,
      .clEnqueueTask = hpcrun_clEnqueueTask,
      .clEnqueueReadBuffer = hpcrun_clEnqueueReadBuffer,
      .clEnqueueWriteBuffer = hpcrun_clEnqueueWriteBuffer,
      .clEnqueueMapBuffer = hpcrun_clEnqueueMapBuffer,
      .clCreateBuffer = hpcrun_clCreateBuffer,
      .clSetKernelArg = hpcrun_clSetKernelArg,
      .clReleaseMemObject = hpcrun_clReleaseMemObject,
      .clReleaseKernel = hpcrun_clReleaseKernel,
      .clWaitForEvents = hpcrun_clWaitForEvents,
      .clReleaseCommandQueue = hpcrun_clReleaseCommandQueue,
      .clFinish = hpcrun_clFinish,
  };
  return &hooks;
}

cl_int f_clBuildProgram(cl_program program, cl_uint num_devices,
                        const cl_device_id* device_list, const char* options,
                        void(CL_CALLBACK* pfn_notify)(cl_program program,
                                                      void* user_data),
                        void* user_data,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clBuildProgram(program, num_devices, device_list, options,
                                  pfn_notify, user_data);
}

cl_int f_clGetProgramInfo(cl_program program, cl_program_info param_name,
                          size_t param_value_size, void* param_value,
                          size_t* param_value_size_ret,
                          const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetProgramInfo(program, param_name, param_value_size, param_value,
                                    param_value_size_ret);
}

cl_int f_clGetKernelInfo(cl_kernel kernel, cl_kernel_info param_name, size_t param_size,
                         void* param_value, size_t* param_size_ret,
                         const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetKernelInfo(kernel, param_name, param_size, param_value,
                                   param_size_ret);
}

cl_int f_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type,
                        cl_uint num_entries, cl_device_id* devices,
                        cl_uint* num_devices,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetDeviceIDs(platform, device_type, num_entries, devices,
                                  num_devices);
}

cl_int f_clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms,
                          cl_uint* num_platforms,
                          const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetPlatformIDs(num_entries, platforms, num_platforms);
}

cl_int f_clRetainEvent(cl_event event,
                       const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clRetainEvent(event);
}

cl_int f_clGetEventInfo(cl_event event, cl_event_info param_name,
                        size_t param_value_size, void* param_value,
                        size_t* param_value_size_ret,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetEventInfo(event, param_name, param_value_size, param_value,
                                  param_value_size_ret);
}

cl_context f_clCreateContext(const cl_context_properties* properties,
                             cl_uint num_devices, const cl_device_id* devices,
                             void(CL_CALLBACK* pfn_notify)(const char* errinfo,
                                                           const void* private_info,
                                                           size_t cb, void* user_data),
                             void* user_data, cl_int* errcode_ret,
                             const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clCreateContext(properties, num_devices, devices, pfn_notify,
                                   user_data, errcode_ret);
}

cl_command_queue
f_clCreateCommandQueue(cl_context ctx, cl_device_id device,
                       cl_command_queue_properties properties, cl_int* err,
                       const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clCreateCommandQueue(ctx, device, properties, err);
}

cl_command_queue f_clCreateCommandQueueWithProperties(
    cl_context ctx, cl_device_id device, const cl_queue_properties* properties,
    cl_int* err, const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clCreateCommandQueueWithProperties(ctx, device, properties, err);
}

cl_int f_clEnqueueNDRangeKernel(cl_command_queue queue, cl_kernel kernel,
                                cl_uint work_dim, const size_t* global_work_offset,
                                const size_t* global_work_size,
                                const size_t* local_work_size,
                                cl_uint num_events_in_wait_list,
                                const cl_event* event_wait_list, cl_event* event,
                                const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clEnqueueNDRangeKernel(
      queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size,
      num_events_in_wait_list, event_wait_list, event);
}

cl_int f_clEnqueueTask(cl_command_queue queue, cl_kernel kernel, cl_uint num_events,
                       const cl_event* events, cl_event* event,
                       const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clEnqueueTask(queue, kernel, num_events, events, event);
}

cl_int f_clEnqueueReadBuffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                             size_t offset, size_t size, void* ptr, cl_uint num_events,
                             const cl_event* events, cl_event* event,
                             const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clEnqueueReadBuffer(queue, buffer, blocking, offset, size, ptr,
                                       num_events, events, event);
}

cl_int f_clEnqueueWriteBuffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                              size_t offset, size_t size, const void* ptr,
                              cl_uint num_events, const cl_event* events,
                              cl_event* event,
                              const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clEnqueueWriteBuffer(queue, buffer, blocking, offset, size, ptr,
                                        num_events, events, event);
}

void* f_clEnqueueMapBuffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                           cl_map_flags flags, size_t offset, size_t size,
                           cl_uint num_events, const cl_event* events, cl_event* event,
                           cl_int* err,
                           const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clEnqueueMapBuffer(queue, buffer, blocking, flags, offset, size,
                                      num_events, events, event, err);
}

cl_mem f_clCreateBuffer(cl_context ctx, cl_mem_flags flags, size_t size, void* host_ptr,
                        cl_int* err,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clCreateBuffer(ctx, flags, size, host_ptr, err);
}

cl_int f_clSetKernelArg(cl_kernel kernel, cl_uint arg_index, size_t arg_size,
                        const void* arg_value,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clSetKernelArg(kernel, arg_index, arg_size, arg_value);
}

cl_int f_clReleaseMemObject(cl_mem mem,
                            const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clReleaseMemObject(mem);
}

cl_int
f_clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name,
                          size_t param_value_size, void* param_value,
                          size_t* param_value_size_ret,
                          const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clGetEventProfilingInfo(event, param_name, param_value_size,
                                           param_value, param_value_size_ret);
}

cl_int f_clReleaseEvent(cl_event event,
                        const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clReleaseEvent(event);
}

cl_int f_clSetEventCallback(cl_event event, cl_int command_exec_callback_type,
                            void(CL_CALLBACK* pfn_notify)(cl_event event,
                                                          cl_int event_command_status,
                                                          void* user_data),
                            void* user_data,
                            const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clSetEventCallback(event, command_exec_callback_type, pfn_notify,
                                      user_data);
}

cl_int f_clReleaseKernel(cl_kernel kernel,
                         const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clReleaseKernel(kernel);
}

cl_int f_clWaitForEvents(cl_uint num_events, const cl_event* event_list,
                         const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clWaitForEvents(num_events, event_list);
}

cl_int f_clReleaseCommandQueue(cl_command_queue command_queue,
                               const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clReleaseCommandQueue(command_queue);
}

cl_int f_clFinish(cl_command_queue command_queue,
                  const struct hpcrun_foil_appdispatch_opencl* dispatch) {
  return dispatch->clFinish(command_queue);
}
