// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "opencl-private.h"

#include <threads.h>

static struct hpcrun_foil_appdispatch_opencl dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_opencl){
      .clGetKernelInfo = foil_dlsym("clGetKernelInfo"),
      .clRetainEvent = foil_dlsym("clRetainEvent"),
      .clReleaseEvent = foil_dlsym("clReleaseEvent"),
      .clGetEventInfo = foil_dlsym("clGetEventInfo"),
      .clGetEventProfilingInfo = foil_dlsym("clGetEventProfilingInfo"),
      .clGetPlatformIDs = foil_dlsym("clGetPlatformIDs"),
      .clGetDeviceIDs = foil_dlsym("clGetDeviceIDs"),
      .clGetProgramInfo = foil_dlsym("clGetProgramInfo"),
      .clSetEventCallback = foil_dlsym("clSetEventCallback"),
      .clBuildProgram = foil_dlsym("clBuildProgram"),
      .clCreateContext = foil_dlsym("clCreateContext"),
      .clCreateCommandQueue = foil_dlsym("clCreateCommandQueue"),
      .clCreateCommandQueueWithProperties =
          foil_dlsym("clCreateCommandQueueWithProperties"),
      .clEnqueueNDRangeKernel = foil_dlsym("clEnqueueNDRangeKernel"),
      .clEnqueueTask = foil_dlsym("clEnqueueTask"),
      .clEnqueueReadBuffer = foil_dlsym("clEnqueueReadBuffer"),
      .clEnqueueWriteBuffer = foil_dlsym("clEnqueueWriteBuffer"),
      .clEnqueueMapBuffer = foil_dlsym("clEnqueueMapBuffer"),
      .clCreateBuffer = foil_dlsym("clCreateBuffer"),
      .clSetKernelArg = foil_dlsym("clSetKernelArg"),
      .clReleaseMemObject = foil_dlsym("clReleaseMemObject"),
      .clReleaseKernel = foil_dlsym("clReleaseKernel"),
      .clWaitForEvents = foil_dlsym("clWaitForEvents"),
      .clReleaseCommandQueue = foil_dlsym("clReleaseCommandQueue"),
      .clFinish = foil_dlsym("clFinish"),
  };
}

static const struct hpcrun_foil_appdispatch_opencl* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API cl_int
clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
               const char* options,
               void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
               void* user_data) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clBuildProgram(
      program, num_devices, device_list, options, pfn_notify, user_data, dispatch());
}

HPCRUN_EXPOSED_API cl_context clCreateContext(
    const cl_context_properties* properties, cl_uint num_devices,
    const cl_device_id* devices,
    void(CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info,
                                  size_t cb, void* user_data),
    void* user_data, cl_int* errcode_ret) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clCreateContext(
      properties, num_devices, devices, pfn_notify, user_data, errcode_ret, dispatch());
}

HPCRUN_EXPOSED_API cl_command_queue
clCreateCommandQueue(cl_context context, cl_device_id device,
                     cl_command_queue_properties properties, cl_int* errcode_ret) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clCreateCommandQueue(
      context, device, properties, errcode_ret, dispatch());
}

HPCRUN_EXPOSED_API cl_command_queue clCreateCommandQueueWithProperties(
    cl_context context, cl_device_id device, const cl_queue_properties* properties,
    cl_int* errcode_ret) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clCreateCommandQueueWithProperties(
      context, device, properties, errcode_ret, dispatch());
}

HPCRUN_EXPOSED_API cl_int clEnqueueNDRangeKernel(
    cl_command_queue command_queue, cl_kernel ocl_kernel, cl_uint work_dim,
    const size_t* global_work_offset, const size_t* global_work_size,
    const size_t* local_work_size, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clEnqueueNDRangeKernel(
      command_queue, ocl_kernel, work_dim, global_work_offset, global_work_size,
      local_work_size, num_events_in_wait_list, event_wait_list, event, dispatch());
}

HPCRUN_EXPOSED_API cl_int clEnqueueTask(cl_command_queue command_queue,
                                        cl_kernel kernel,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event* event_wait_list,
                                        cl_event* event) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clEnqueueTask(
      command_queue, kernel, num_events_in_wait_list, event_wait_list, event,
      dispatch());
}

HPCRUN_EXPOSED_API cl_int clEnqueueReadBuffer(cl_command_queue command_queue,
                                              cl_mem buffer, cl_bool blocking_read,
                                              size_t offset, size_t cb, void* ptr,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event* event_wait_list,
                                              cl_event* event) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clEnqueueReadBuffer(
      command_queue, buffer, blocking_read, offset, cb, ptr, num_events_in_wait_list,
      event_wait_list, event, dispatch());
}

HPCRUN_EXPOSED_API cl_int clEnqueueWriteBuffer(
    cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
    size_t offset, size_t cb, const void* ptr, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clEnqueueWriteBuffer(
      command_queue, buffer, blocking_write, offset, cb, ptr, num_events_in_wait_list,
      event_wait_list, event, dispatch());
}

HPCRUN_EXPOSED_API void*
clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
                   cl_map_flags map_flags, size_t offset, size_t size,
                   cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                   cl_event* event, cl_int* errcode_ret) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clEnqueueMapBuffer(
      command_queue, buffer, blocking_map, map_flags, offset, size,
      num_events_in_wait_list, event_wait_list, event, errcode_ret, dispatch());
}

HPCRUN_EXPOSED_API cl_mem clCreateBuffer(cl_context context, cl_mem_flags flags,
                                         size_t size, void* host_ptr,
                                         cl_int* errcode_ret) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clCreateBuffer(
      context, flags, size, host_ptr, errcode_ret, dispatch());
}

HPCRUN_EXPOSED_API cl_int clSetKernelArg(cl_kernel kernel, cl_uint arg_index,
                                         size_t arg_size, const void* arg_value) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clSetKernelArg(
      kernel, arg_index, arg_size, arg_value, dispatch());
}

HPCRUN_EXPOSED_API cl_int clReleaseMemObject(cl_mem mem) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clReleaseMemObject(mem, dispatch());
}

HPCRUN_EXPOSED_API cl_int clReleaseKernel(cl_kernel kernel) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clReleaseKernel(kernel, dispatch());
}

// comment if opencl blame-shifting is disabled
HPCRUN_EXPOSED_API cl_int clWaitForEvents(cl_uint num_events,
                                          const cl_event* event_list) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clWaitForEvents(num_events, event_list,
                                                              dispatch());
}

HPCRUN_EXPOSED_API cl_int clReleaseCommandQueue(cl_command_queue command_queue) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clReleaseCommandQueue(command_queue,
                                                                    dispatch());
}

HPCRUN_EXPOSED_API cl_int clFinish(cl_command_queue command_queue) {
  return hpcrun_foil_fetch_hooks_opencl_dl()->clFinish(command_queue, dispatch());
}
