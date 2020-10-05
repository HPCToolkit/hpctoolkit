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

#include <assert.h>
#include <inttypes.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/instrumentation/gtpin-instrumentation.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/files.h>
#include <lib/prof-lean/hpcrun-opencl.h>
#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/usec_time.h>

#include "opencl-api.h"
#include "opencl-activity-translate.h"
#include "opencl-memory-manager.h"



//******************************************************************************
// macros
//******************************************************************************

#define CPU_NANOTIME() (usec_time() * 1000)

#define FORALL_OPENCL_ERRORS(macro)          \
  macro(CL_SUCCESS)              \
  macro(CL_DEVICE_NOT_FOUND)            \
  macro(CL_DEVICE_NOT_AVAILABLE)          \
  macro(CL_COMPILER_NOT_AVAILABLE)          \
  macro(CL_MEM_OBJECT_ALLOCATION_FAILURE)        \
  macro(CL_OUT_OF_RESOURCES)            \
  macro(CL_OUT_OF_HOST_MEMORY)            \
  macro(CL_PROFILING_INFO_NOT_AVAILABLE)        \
  macro(CL_MEM_COPY_OVERLAP)            \
  macro(CL_IMAGE_FORMAT_MISMATCH)          \
  macro(CL_IMAGE_FORMAT_NOT_SUPPORTED)          \
  macro(CL_BUILD_PROGRAM_FAILURE)          \
  macro(CL_MAP_FAILURE)              \
  macro(CL_MISALIGNED_SUB_BUFFER_OFFSET)        \
  macro(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST)      \
  macro(CL_COMPILE_PROGRAM_FAILURE)          \
  macro(CL_LINKER_NOT_AVAILABLE)          \
  macro(CL_LINK_PROGRAM_FAILURE)          \
  macro(CL_DEVICE_PARTITION_FAILED)          \
  macro(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)        \
  macro(CL_INVALID_VALUE)            \
  macro(CL_INVALID_DEVICE_TYPE)            \
  macro(CL_INVALID_PLATFORM)            \
  macro(CL_INVALID_DEVICE)            \
  macro(CL_INVALID_CONTEXT)            \
  macro(CL_INVALID_QUEUE_PROPERTIES)          \
  macro(CL_INVALID_COMMAND_QUEUE)          \
  macro(CL_INVALID_HOST_PTR)            \
  macro(CL_INVALID_MEM_OBJECT)            \
  macro(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)        \
  macro(CL_INVALID_IMAGE_SIZE)            \
  macro(CL_INVALID_SAMPLER)            \
  macro(CL_INVALID_BINARY)            \
  macro(CL_INVALID_BUILD_OPTIONS)          \
  macro(CL_INVALID_PROGRAM)            \
  macro(CL_INVALID_PROGRAM_EXECUTABLE)          \
  macro(CL_INVALID_KERNEL_NAME)            \
  macro(CL_INVALID_KERNEL_DEFINITION)          \
  macro(CL_INVALID_KERNEL)            \
  macro(CL_INVALID_ARG_INDEX)            \
  macro(CL_INVALID_ARG_VALUE)            \
  macro(CL_INVALID_ARG_SIZE)            \
  macro(CL_INVALID_KERNEL_ARGS)            \
  macro(CL_INVALID_WORK_DIMENSION)          \
  macro(CL_INVALID_WORK_GROUP_SIZE)          \
  macro(CL_INVALID_WORK_ITEM_SIZE)          \
  macro(CL_INVALID_GLOBAL_OFFSET)          \
  macro(CL_INVALID_EVENT_WAIT_LIST)          \
  macro(CL_INVALID_EVENT)            \
  macro(CL_INVALID_OPERATION)            \
  macro(CL_INVALID_GL_OBJECT)            \
  macro(CL_INVALID_BUFFER_SIZE)            \
  macro(CL_INVALID_MIP_LEVEL)            \
  macro(CL_INVALID_GLOBAL_WORK_SIZE)          \
  macro(CL_INVALID_PROPERTY)            \
  macro(CL_INVALID_IMAGE_DESCRIPTOR)          \
  macro(CL_INVALID_COMPILER_OPTIONS)          \
  macro(CL_INVALID_LINKER_OPTIONS)          \
  macro(CL_INVALID_DEVICE_PARTITION_COUNT)

#define FORALL_OPENCL_CALLS(macro)          \
  macro(memcpy_H2D)              \
  macro(memcpy_D2H)              \
  macro(kernel)

#define CODE_TO_STRING(e) case e: return #e;

#define opencl_path() "libOpenCL.so"

#define FORALL_OPENCL_ROUTINES(macro)  \
  macro(clBuildProgram)  \
  macro(clCreateProgramWithSource)  \
  macro(clCreateCommandQueue)  \
  macro(clCreateCommandQueueWithProperties)  \
  macro(clEnqueueNDRangeKernel)  \
  macro(clEnqueueReadBuffer)  \
  macro(clEnqueueWriteBuffer)  \
  macro(clGetEventProfilingInfo)  \
  macro(clReleaseEvent)  \
  macro(clSetEventCallback)

#define OPENCL_FN_NAME(f) DYN_FN_NAME(f)

#define OPENCL_FN(fn, args)      \
  static cl_int (*OPENCL_FN_NAME(fn)) args

#define OPENCL_PROGRAM_FN(fn, args)      \
  static cl_program (*OPENCL_FN_NAME(fn)) args

#define OPENCL_QUEUE_FN(fn, args)      \
  static cl_command_queue (*OPENCL_FN_NAME(fn)) args

#define HPCRUN_OPENCL_CALL(fn, args) (OPENCL_FN_NAME(fn) args)

#define LINE_TABLE_FLAG " -gline-tables-only "

//******************************************************************************
// local data
//******************************************************************************

//----------------------------------------------------------
// opencl function pointers for late binding
//----------------------------------------------------------

OPENCL_FN
(
  clBuildProgram, 
  (
   cl_program program,
   cl_uint num_devices,
   const cl_device_id* device_list,
   const char* options,
   void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
   void* user_data
  )
);


OPENCL_PROGRAM_FN
(
  clCreateProgramWithSource, 
  (
   cl_context context,
   cl_uint count,
   const char** strings,
   const size_t* lengths,
   cl_int* errcode_ret
  )
);


OPENCL_QUEUE_FN
(
  clCreateCommandQueue, 
  (
   cl_context,
   cl_device_id,
   cl_command_queue_properties,
   cl_int*
  )
);


OPENCL_QUEUE_FN
(
  clCreateCommandQueueWithProperties, 
  (
   cl_context,
   cl_device_id,
   const cl_bitfield *,
   cl_int*
  )
);


OPENCL_FN
(
  clEnqueueNDRangeKernel, 
  (
   cl_command_queue,
   cl_kernel,
   cl_uint,
   const size_t *, 
   const size_t *,
   const size_t *,
   cl_uint,
   const cl_event *,
   cl_event *
  )
);


OPENCL_FN
(
  clEnqueueReadBuffer, 
  (
   cl_command_queue,
   cl_mem,
   cl_bool,
   size_t,
   size_t,
   void *,
   cl_uint,
   const cl_event *,
   cl_event *
  )
);


OPENCL_FN
(
  clEnqueueWriteBuffer, 
  (
   cl_command_queue,
   cl_mem,
   cl_bool,
   size_t,
   size_t,
   const void *,
   cl_uint,
   const cl_event *,
   cl_event *
  )
);


OPENCL_FN
(
  clGetEventProfilingInfo,
  (
   cl_event event,
   cl_profiling_info param_name,
   size_t param_value_size,
   void *param_value,
   size_t *param_value_size_ret
  )
);


OPENCL_FN
(
  clReleaseEvent, 
  (
   cl_event event
  )
);


OPENCL_FN
(
  clSetEventCallback,
  (
   cl_event event,
   cl_int command_exec_callback_type,
   void (CL_CALLBACK *pfn_notify)
   (cl_event event, cl_int event_command_status, void *user_data),
   void *user_data
  )
);


static atomic_ullong opencl_pending_operations;
static atomic_long correlation_id;
static bool instrumentation = false;

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100



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


// we are dumping the debuginfo since the binary does not have debugsection
static void
clBuildProgramCallback
(
 cl_program program,
 void* user_data
)
{
}


static void
opencl_pending_operations_adjust
(
 int value
)
{
  atomic_fetch_add(&opencl_pending_operations, value);
}


static void
opencl_activity_completion_notify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
opencl_activity_process
(
 cl_event event,
 void *user_data
)
{
  gpu_activity_t gpu_activity;
  opencl_activity_translate(&gpu_activity, event, user_data);
  gpu_activity_process(&gpu_activity);
}


static void
opencl_wait_for_pending_operations
(
  void
)
{
  ETMSG(OPENCL, "pending operations: %lu", 
  atomic_load(&opencl_pending_operations));
  while (atomic_load(&opencl_pending_operations) != 0);
}


static const char*
opencl_call_to_string
(
  opencl_call_t type
)
{
  switch (type)
  {
    FORALL_OPENCL_CALLS(CODE_TO_STRING)
    default: return "CL_unknown_call";
  }
}


__attribute__((unused))
static const char*
opencl_error_report
(
  cl_int error_status
)
{
  switch(error_status) {
    FORALL_OPENCL_ERRORS(CODE_TO_STRING)
    default: return "Unknown OpenCL error";
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_subscriber_callback
(
 opencl_call_t type,
 uint64_t correlation_id
)
{
  opencl_pending_operations_adjust(1);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_ccts_t gpu_op_ccts;
  gpu_correlation_id_map_insert(correlation_id, correlation_id);
  cct_node_t *api_node = 
    gpu_application_thread_correlation_callback(correlation_id);

  switch (type) {
    case memcpy_H2D:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
           gpu_placeholder_type_copyin);
      break;
    case memcpy_D2H:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
           gpu_placeholder_type_copyout);
      break;
    case kernel:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
           gpu_placeholder_type_kernel);

      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
           gpu_placeholder_type_trace);
      break;
    default:
      assert(0);
  }

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  hpcrun_safe_exit();

  if (instrumentation && type == kernel) {
    // Callback to produce gtpin correlation
    gtpin_produce_runtime_callstack(&gpu_op_ccts);
  }

  gpu_activity_channel_consume(gpu_metrics_attribute);  
  uint64_t cpu_submit_time = CPU_NANOTIME();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, 
          cpu_submit_time);
}


void
opencl_activity_completion_callback
(
 cl_event event,
 cl_int event_command_exec_status,
 void *user_data
)
{
  cl_int complete_flag = CL_COMPLETE;
  opencl_object_t *o = (opencl_object_t*)user_data;
  cl_generic_callback_t *act_data;
  if (o->kind == OPENCL_KERNEL_CALLBACK) {
    act_data = (cl_generic_callback_t*) &(o->details.ker_cb);
  } else if (o->kind == OPENCL_MEMORY_CALLBACK) {
    act_data = (cl_generic_callback_t*) &(o->details.mem_cb);
  }
  uint64_t correlation_id = act_data->correlation_id;
  opencl_call_t type = act_data->type;

  if (event_command_exec_status == complete_flag) {
    gpu_correlation_id_map_entry_t *cid_map_entry = 
      gpu_correlation_id_map_lookup(correlation_id);
    if (cid_map_entry == NULL) {
      ETMSG(OPENCL, "completion callback was called before registration " 
      "callback. type: %d, correlation: %"PRIu64 "", type, 
      correlation_id);
    }
    ETMSG(OPENCL, "completion type: %s, Correlation id: %"PRIu64 "", 
    opencl_call_to_string(type), correlation_id);
    opencl_activity_completion_notify();
    opencl_activity_process(event, act_data);
  }
  if (o->isInternalClEvent) {
    HPCRUN_OPENCL_CALL(clReleaseEvent, (event));
  }
  opencl_free(o);
  opencl_pending_operations_adjust(-1);
}


void
opencl_timing_info_get
(
 gpu_interval_t *interval,
 cl_event event
)
{
  cl_ulong commandStart = 0;
  cl_ulong commandEnd = 0;

  HPCRUN_OPENCL_CALL(clGetEventProfilingInfo, 
         (event, CL_PROFILING_COMMAND_START, 
          sizeof(commandStart), &commandStart, NULL));

  HPCRUN_OPENCL_CALL(clGetEventProfilingInfo, 
         (event, CL_PROFILING_COMMAND_END, 
          sizeof(commandEnd), &commandEnd, NULL));

  ETMSG(OPENCL, "duration [%lu, %lu]", commandStart, commandEnd);

  set_gpu_interval(interval, (uint64_t)commandStart, (uint64_t)commandEnd);
}


void
clSetEventCallback_wrapper
(
 cl_event event,
 cl_int event_command_status,
 void (CL_CALLBACK *pfn_notify)
 (cl_event event, cl_int event_command_status, void *user_data),
 void *user_data
)
{
  HPCRUN_OPENCL_CALL(clSetEventCallback, 
         (event, event_command_status, pfn_notify, user_data));
}


void
opencl_api_initialize
(
 void
)
{
	if (instrumentation) {
  	gpu_metrics_GPU_INST_enable();
  	gtpin_enable_profiling();
	}
  atomic_store(&correlation_id, 0);
  atomic_store(&opencl_pending_operations, 0);
}


int
opencl_bind
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK  
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(opencl, opencl_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);
  
#define OPENCL_BIND(fn)        \
  CHK_DLSYM(opencl, fn);
  
  FORALL_OPENCL_ROUTINES(OPENCL_BIND)
    
#undef OPENCL_BIND
    
    return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK  
}


cl_program
clCreateProgramWithSource
(
 cl_context context,
 cl_uint count,
 const char** strings,
 const size_t* lengths,
 cl_int* errcode_ret
)
{
  ETMSG(OPENCL, "inside clCreateProgramWithSource_wrapper");

  FILE *f_ptr;
  for (int i = 0; i < (int)count; i++) {
    // what if a single file has multiple kernels?
    // we need to add logic to get filenames by reading the strings contents
    char fileno = '0' + (i + 1); // right now we are naming the files as index numbers
    // using malloc instead of hpcrun_malloc gives extra garbage characters in file name
    char *filename = (char*)hpcrun_malloc(sizeof(fileno) + 1);
    *filename = fileno + '\0';
    f_ptr = fopen(filename, "w");
    fwrite(strings[i], lengths[i], 1, f_ptr);
  }
  fclose(f_ptr);
  
  return HPCRUN_OPENCL_CALL(clCreateProgramWithSource, (context, count, strings, lengths, errcode_ret));
}


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
  ETMSG(OPENCL, "inside clBuildProgram_wrapper");
  // XXX(Aaron): Caution, what's the maximum length of options?
  int len_options = options == NULL ? 0 : strlen(options);
  int len_flag = strlen(LINE_TABLE_FLAG);
  char *options_with_debug_flags = (char *)malloc((len_options + len_flag + 1) * sizeof(char));
  memset(options_with_debug_flags, 0, (len_options + len_flag + 1));
  if (len_options != 0) {
    strncat(options_with_debug_flags, options, len_options);
  }
  strcat(options_with_debug_flags, LINE_TABLE_FLAG);
  cl_int ret = HPCRUN_OPENCL_CALL(clBuildProgram, (program, num_devices, device_list, options_with_debug_flags, clBuildProgramCallback, user_data));
  free(options_with_debug_flags);
  return ret;
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
  // enabling profiling
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; 

  return HPCRUN_OPENCL_CALL(clCreateCommandQueue, (context, device,
        properties,errcode_ret));  
}


cl_command_queue
clCreateCommandQueueWithProperties
(
 cl_context context,
 cl_device_id device,
 const cl_bitfield* properties,
 cl_int* errcode_ret
)
{
  cl_bitfield *queue_properties = (cl_bitfield *)properties;
  if (properties == NULL) {
    queue_properties = (cl_bitfield *)malloc(sizeof(cl_bitfield) * 3);
    queue_properties[0] = CL_QUEUE_PROPERTIES;
    queue_properties[1] = CL_QUEUE_PROFILING_ENABLE;
    queue_properties[2] = 0;
  } else {
    int queue_props_id = -1;
    int props_count = 0;
    while (properties[props_count] != 0) {
      if (properties[props_count] == CL_QUEUE_PROPERTIES) {
        queue_props_id = props_count;
        ++props_count;
      } else if (properties[props_count] == 0x1094) {
        // TODO(Keren): A temporay hack
        ++props_count;
      }
      ++props_count;
    }

    if (queue_props_id >= 0 && queue_props_id + 1 < props_count) {
      queue_properties = (cl_bitfield *)malloc(sizeof(cl_bitfield) * (props_count + 1));
      for (int i = 0; i < props_count; ++i) {
        queue_properties[i] = properties[i];
      }
      // We do have a queue property entry, just enable profiling
      queue_properties[queue_props_id + 1] |= CL_QUEUE_PROFILING_ENABLE;
      queue_properties[props_count] = 0;
    } else {
      // We do not have a queue property entry, need to allocate a queue property entry and set up
      queue_properties = (cl_bitfield *)malloc(sizeof(cl_bitfield) * (props_count + 3));
      for (int i = 0; i < props_count; ++i) {
        queue_properties[i] = properties[i];
      }
      queue_properties[props_count] = CL_QUEUE_PROPERTIES;
      queue_properties[props_count + 1] = CL_QUEUE_PROFILING_ENABLE;
      queue_properties[props_count + 2] = 0;
    }
  }
  cl_command_queue queue = HPCRUN_OPENCL_CALL(clCreateCommandQueueWithProperties, (context, device, queue_properties, errcode_ret));
  if (queue_properties != NULL) {
    // The property is created by us
    free(queue_properties);
  }
  return queue;
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
  cl_int return_status = 
    HPCRUN_OPENCL_CALL(clEnqueueNDRangeKernel, (command_queue, ocl_kernel, work_dim, 
           global_work_offset, global_work_size, 
           local_work_size, num_events_in_wait_list, 
           event_wait_list, eventp));

  ETMSG(OPENCL, "registering callback for type: kernel. " 
  "Correlation id: %"PRIu64 "", correlation_id);

  opencl_subscriber_callback(kernel_cb->type, kernel_cb->correlation_id);
  clSetEventCallback_wrapper(*eventp, CL_COMPLETE, 
           &opencl_activity_completion_callback, kernel_info);
  return return_status;
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
  cl_int return_status = 
    HPCRUN_OPENCL_CALL(clEnqueueReadBuffer, (command_queue, buffer, blocking_read, offset, 
        cb, ptr, num_events_in_wait_list, 
        event_wait_list, eventp));

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
  cl_int return_status = 
    HPCRUN_OPENCL_CALL(clEnqueueWriteBuffer, (command_queue, buffer, blocking_write, offset,
         cb, ptr, num_events_in_wait_list, 
         event_wait_list, eventp));

  ETMSG(OPENCL, "registering callback for type: H2D. " 
  "Correlation id: %"PRIu64 "", correlation_id);

  ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device", 
  (long)cb);

  opencl_subscriber_callback(mem_transfer_cb->type, 
           mem_transfer_cb->correlation_id);

  clSetEventCallback_wrapper(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, (void *)mem_info);

  return return_status;
}


void
opencl_enable_instrumentation
(
	void
)
{
	instrumentation = true;
}


void
opencl_api_finalize
(
 void *args
)
{
  opencl_wait_for_pending_operations();
  gpu_application_thread_process_activities();
}
