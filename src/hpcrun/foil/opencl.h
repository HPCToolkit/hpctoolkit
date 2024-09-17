// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_OPENCL_H
#define HPCRUN_FOIL_OPENCL_H

#include <CL/cl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hpcrun_foil_appdispatch_opencl;

cl_int f_clGetKernelInfo(cl_kernel, cl_kernel_info, size_t, void*, size_t*,
                         const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clRetainEvent(cl_event, const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clReleaseEvent(cl_event, const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clGetEventInfo(cl_event, cl_event_info, size_t, void*, size_t*,
                        const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clGetEventProfilingInfo(cl_event, cl_profiling_info, size_t, void*, size_t*,
                                 const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clGetPlatformIDs(cl_uint, cl_platform_id*, cl_uint*,
                          const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clGetDeviceIDs(cl_platform_id, cl_device_type, cl_uint, cl_device_id*,
                        cl_uint*, const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clGetProgramInfo(cl_program, cl_program_info, size_t, void*, size_t*,
                          const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clSetEventCallback(cl_event, cl_int, void (*)(cl_event, cl_int, void*), void*,
                            const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clBuildProgram(cl_program, cl_uint, const cl_device_id*, const char*,
                        void (*)(cl_program, void*), void*,
                        const struct hpcrun_foil_appdispatch_opencl*);
cl_context f_clCreateContext(const cl_context_properties*, cl_uint, const cl_device_id*,
                             void (*)(const char*, const void*, size_t, void*), void*,
                             cl_int*, const struct hpcrun_foil_appdispatch_opencl*);
cl_command_queue f_clCreateCommandQueue(cl_context, cl_device_id,
                                        cl_command_queue_properties, cl_int*,
                                        const struct hpcrun_foil_appdispatch_opencl*);
cl_command_queue
f_clCreateCommandQueueWithProperties(cl_context, cl_device_id,
                                     const cl_queue_properties*, cl_int*,
                                     const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clEnqueueNDRangeKernel(cl_command_queue, cl_kernel, cl_uint, const size_t*,
                                const size_t*, const size_t*, cl_uint, const cl_event*,
                                cl_event*,
                                const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clEnqueueTask(cl_command_queue, cl_kernel, cl_uint, const cl_event*, cl_event*,
                       const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clEnqueueReadBuffer(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*,
                             cl_uint, const cl_event*, cl_event*,
                             const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clEnqueueWriteBuffer(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                              const void*, cl_uint, const cl_event*, cl_event*,
                              const struct hpcrun_foil_appdispatch_opencl*);
void* f_clEnqueueMapBuffer(cl_command_queue, cl_mem, cl_bool, cl_map_flags, size_t,
                           size_t, cl_uint, const cl_event*, cl_event*, cl_int*,
                           const struct hpcrun_foil_appdispatch_opencl*);
cl_mem f_clCreateBuffer(cl_context, cl_mem_flags, size_t, void*, cl_int*,
                        const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clSetKernelArg(cl_kernel, cl_uint, size_t, const void*,
                        const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clReleaseMemObject(cl_mem, const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clReleaseKernel(cl_kernel, const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clWaitForEvents(cl_uint, const cl_event*,
                         const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clReleaseCommandQueue(cl_command_queue,
                               const struct hpcrun_foil_appdispatch_opencl*);
cl_int f_clFinish(cl_command_queue, const struct hpcrun_foil_appdispatch_opencl*);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_OPENCL_H
