// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_OPENCL_PRIVATE_H
#define HPCRUN_FOIL_OPENCL_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "opencl.h"

struct hpcrun_foil_appdispatch_opencl {
  cl_int (*clGetKernelInfo)(cl_kernel, cl_kernel_info, size_t, void*, size_t*);
  cl_int (*clRetainEvent)(cl_event);
  cl_int (*clReleaseEvent)(cl_event);
  cl_int (*clGetEventInfo)(cl_event, cl_event_info, size_t, void*, size_t*);
  cl_int (*clGetEventProfilingInfo)(cl_event, cl_profiling_info, size_t, void*,
                                    size_t*);
  cl_int (*clGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
  cl_int (*clGetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*,
                           cl_uint*);
  cl_int (*clGetProgramInfo)(cl_program, cl_program_info, size_t, void*, size_t*);
  cl_int (*clSetEventCallback)(cl_event, cl_int, void (*)(cl_event, cl_int, void*),
                               void*);
  cl_int (*clBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
                           void (*)(cl_program, void*), void*);
  cl_context (*clCreateContext)(const cl_context_properties*, cl_uint,
                                const cl_device_id*,
                                void (*)(const char*, const void*, size_t, void*),
                                void*, cl_int*);
  cl_command_queue (*clCreateCommandQueue)(cl_context, cl_device_id,
                                           cl_command_queue_properties, cl_int*);
  cl_command_queue (*clCreateCommandQueueWithProperties)(cl_context, cl_device_id,
                                                         const cl_queue_properties*,
                                                         cl_int*);
  cl_int (*clEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*,
                                   const size_t*, const size_t*, cl_uint,
                                   const cl_event*, cl_event*);
  cl_int (*clEnqueueTask)(cl_command_queue, cl_kernel, cl_uint, const cl_event*,
                          cl_event*);
  cl_int (*clEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                void*, cl_uint, const cl_event*, cl_event*);
  cl_int (*clEnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                 const void*, cl_uint, const cl_event*, cl_event*);
  void* (*clEnqueueMapBuffer)(cl_command_queue, cl_mem, cl_bool, cl_map_flags, size_t,
                              size_t, cl_uint, const cl_event*, cl_event*, cl_int*);
  cl_mem (*clCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
  cl_int (*clSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
  cl_int (*clReleaseMemObject)(cl_mem);
  cl_int (*clReleaseKernel)(cl_kernel);
  cl_int (*clWaitForEvents)(cl_uint, const cl_event*);
  cl_int (*clReleaseCommandQueue)(cl_command_queue);
  cl_int (*clFinish)(cl_command_queue);
};

struct hpcrun_foil_hookdispatch_opencl {
  cl_int (*clBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
                           void (*)(cl_program, void*), void*,
                           const struct hpcrun_foil_appdispatch_opencl*);
  cl_context (*clCreateContext)(const cl_context_properties*, cl_uint,
                                const cl_device_id*,
                                void (*)(const char*, const void*, size_t, void*),
                                void*, cl_int*,
                                const struct hpcrun_foil_appdispatch_opencl*);
  cl_command_queue (*clCreateCommandQueue)(cl_context, cl_device_id,
                                           cl_command_queue_properties, cl_int*,
                                           const struct hpcrun_foil_appdispatch_opencl*);
  cl_command_queue (*clCreateCommandQueueWithProperties)(
      cl_context, cl_device_id, const cl_queue_properties*, cl_int*,
      const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*,
                                   const size_t*, const size_t*, cl_uint,
                                   const cl_event*, cl_event*,
                                   const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clEnqueueTask)(cl_command_queue, cl_kernel, cl_uint, const cl_event*,
                          cl_event*, const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                void*, cl_uint, const cl_event*, cl_event*,
                                const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clEnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                 const void*, cl_uint, const cl_event*, cl_event*,
                                 const struct hpcrun_foil_appdispatch_opencl*);
  void* (*clEnqueueMapBuffer)(cl_command_queue, cl_mem, cl_bool, cl_map_flags, size_t,
                              size_t, cl_uint, const cl_event*, cl_event*, cl_int*,
                              const struct hpcrun_foil_appdispatch_opencl*);
  cl_mem (*clCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*,
                           const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clSetKernelArg)(cl_kernel, cl_uint, size_t, const void*,
                           const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clReleaseMemObject)(cl_mem, const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clReleaseKernel)(cl_kernel, const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clWaitForEvents)(cl_uint, const cl_event*,
                            const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clReleaseCommandQueue)(cl_command_queue,
                                  const struct hpcrun_foil_appdispatch_opencl*);
  cl_int (*clFinish)(cl_command_queue, const struct hpcrun_foil_appdispatch_opencl*);
};

#endif // HPCRUN_FOIL_OPENCL_PRIVATE_H
