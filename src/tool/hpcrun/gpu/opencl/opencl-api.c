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
// system includes
//******************************************************************************

#include <assert.h>
#include <inttypes.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <pthread.h>   // pthread_once
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
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
#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <lib/prof-lean/hpcrun-opencl.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/usec_time.h>

#include "opencl-api.h"
#include "opencl-activity-translate.h"
#include "opencl-memory-manager.h"
#include "opencl-h2d-map.h"
#include "opencl-queue-map.h"
#include "opencl-context-map.h"


//******************************************************************************
// macros
//******************************************************************************

#define CPU_NANOTIME() (usec_time() * 1000)

#define opencl_path() "libOpenCL.so"

#define FORALL_OPENCL_ROUTINES(macro)  \
  macro(clBuildProgram)  \
  macro(clCreateCommandQueue)  \
  macro(clCreateCommandQueueWithProperties)  \
  macro(clEnqueueNDRangeKernel)  \
  macro(clEnqueueTask)  \
  macro(clEnqueueReadBuffer)  \
  macro(clEnqueueWriteBuffer)  \
  macro(clEnqueueMapBuffer) \
  macro(clCreateBuffer)  \
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

#define OPENCL_ENQUEUEMAPBUFFER_FN(fn, args)    \
  static void* (*OPENCL_FN_NAME(fn)) args

#define OPENCL_CREATEBUFFER_FN(fn, args)      \
  static cl_mem (*OPENCL_FN_NAME(fn)) args

#define HPCRUN_OPENCL_CALL(fn, args) (OPENCL_FN_NAME(fn) args)

#define LINE_TABLE_FLAG " -gline-tables-only "

#define CORRELATION_ID_INVALID -1

#define BUFFER_ID_INVALID -1

#define DEFAULT_OPENCL_STREAM_ID 0

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100

#define OPENCL_BIND_RESULT_SUCCESS         0
#define OPENCL_BIND_RESULT_FAILURE        -1
#define OPENCL_BIND_RESULT_UNINITIALIZED  -2


//******************************************************************************
// local data
//******************************************************************************

static atomic_ullong opencl_h2d_pending_operations;
static atomic_uint correlation_id_counter = { 0 };
// Global pending operation count for all threads
static atomic_uint opencl_pending_operations = { 0 };

// The thread itself how many pending operations
static __thread atomic_int opencl_self_pending_operations = { 0 };
// Mark if a thread has invoked any opencl call
// If yes, we can flush all opencl activities when the thread terminates
static __thread bool opencl_api_flag = false;

static spinlock_t opencl_h2d_lock = SPINLOCK_UNLOCKED;
static bool instrumentation = false;

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
   const cl_queue_properties *,
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
  clEnqueueTask,
  (
   cl_command_queue,
   cl_kernel,
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


OPENCL_ENQUEUEMAPBUFFER_FN
(
  clEnqueueMapBuffer,
  (
   cl_command_queue,
   cl_mem,
   cl_bool,
   cl_map_flags,
   size_t,
   size_t,
   cl_uint,
   const cl_event*,
   cl_event*,
   cl_int*
  )
);


OPENCL_CREATEBUFFER_FN
(
  clCreateBuffer,
  (
   cl_context,
   cl_mem_flags,
   size_t,
   void *,
   cl_int *
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



//******************************************************************************
// private operations
//******************************************************************************

static uint32_t
getCorrelationId
(
 void
)
{
  return atomic_fetch_add(&correlation_id_counter, 1);
}


static void
initializeKernelCallBackInfo
(
 opencl_object_t *ker_info,
 cl_command_queue command_queue
)
{
  opencl_queue_map_entry_t *qe = opencl_cl_queue_map_lookup((uint64_t)command_queue);
  uint32_t context_id = opencl_cl_queue_map_entry_context_id_get(qe);
  uint32_t queue_id = opencl_cl_queue_map_entry_queue_id_get(qe);

  ETMSG(OPENCL, "submit kernel to context %u queue %u\n", context_id, queue_id);

  ker_info->details.context_id = context_id;
  ker_info->details.stream_id = queue_id;
  ker_info->pending_operations = &opencl_self_pending_operations;
}


static void
initializeMemcpyCallBackInfo
(
 opencl_object_t *cpy_info,
 gpu_memcpy_type_t type,
 size_t size,
 cl_command_queue command_queue
)
{
  opencl_queue_map_entry_t *qe = opencl_cl_queue_map_lookup((uint64_t)command_queue);
  uint32_t context_id = opencl_cl_queue_map_entry_context_id_get(qe);
  uint32_t queue_id = opencl_cl_queue_map_entry_queue_id_get(qe);

  ETMSG(OPENCL, "submit memcpy to context %u queue %u\n", context_id, queue_id);

  cpy_info->kind = GPU_ACTIVITY_MEMCPY;
  cpy_info->details.cpy_cb.type = type;
  cpy_info->details.cpy_cb.fromHostToDevice = (type == GPU_MEMCPY_H2D);
  cpy_info->details.cpy_cb.fromDeviceToHost = (type == GPU_MEMCPY_D2H);
  cpy_info->details.cpy_cb.size = size;
  cpy_info->details.context_id = context_id;
  cpy_info->details.stream_id = queue_id;
  cpy_info->pending_operations = &opencl_self_pending_operations;
}


static void
initializeMemoryCallBackInfo
(
 opencl_object_t *mem_info,
 cl_mem_flags flags,
 size_t size
)
{
  mem_info->kind = GPU_ACTIVITY_MEMORY;
  if (flags & CL_MEM_USE_HOST_PTR) {
    // Managed by the host
    mem_info->details.mem_cb.type = GPU_MEM_MANAGED;
  } else if (flags & CL_MEM_ALLOC_HOST_PTR) {
    // Use host memory
    mem_info->details.mem_cb.type = GPU_MEM_PINNED;
  } else {
    // Normal
    mem_info->details.mem_cb.type = GPU_MEM_DEVICE;
  }

  mem_info->details.mem_cb.size = size;
}


#define INITIALIZE_CALLBACK_INFO(f, obj, args) \
  f args; \
  obj->pending_operations = &opencl_self_pending_operations;


#define SET_EVENT_POINTER(eventp, event, obj) \
  cl_event my_event; \
  if (!event) { \
    obj->internal_event = true; \
    eventp = &my_event; \
  } else { \
    eventp = event; \
    obj->internal_event = false; \
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


static void __attribute__((unused))
opencl_h2d_pending_operations_adjust
(
 int value
)
{
  atomic_fetch_add(&opencl_h2d_pending_operations, value);
}


static void
opencl_operation_multiplexer_push
(
 gpu_interval_t interval,
 opencl_object_t *obj,
 uint32_t correlation_id
)
{
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  // A pseudo host correlation entry
  gpu_activity.kind = GPU_ACTIVITY_EXTERNAL_CORRELATION;
  gpu_activity.details.correlation.correlation_id = correlation_id;
  gpu_activity.details.correlation.host_correlation_id = correlation_id;
  gpu_operation_multiplexer_push(obj->details.initiator_channel,
    NULL, &gpu_activity);

  // The actual entry
  opencl_activity_translate(&gpu_activity, obj, interval);
  gpu_operation_multiplexer_push(obj->details.initiator_channel,
    obj->pending_operations, &gpu_activity);
}


static void
opencl_activity_process
(
 cl_event event,
 opencl_object_t *obj,
 uint32_t correlation_id
)
{
  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));
  opencl_timing_info_get(&interval, event);

  opencl_operation_multiplexer_push(interval, obj, correlation_id);
}


static void __attribute__((unused))
opencl_clSetKernelArg_activity_process
(
 opencl_h2d_map_entry_t *entry,
 opencl_object_t *cb_data
)
{
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  uint32_t correlation_id = opencl_h2d_map_entry_correlation_get(entry);
  opencl_h2d_map_entry_size_get(entry);
  cb_data->details.ker_cb.correlation_id = correlation_id;

  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));

  opencl_activity_translate(&gpu_activity, cb_data, interval);

  ETMSG(OPENCL, "cb_data->details.initiator_channel: %p", cb_data->details.initiator_channel);
  gpu_operation_multiplexer_push(cb_data->details.initiator_channel,
    cb_data->pending_operations, &gpu_activity);
}


static uint64_t __attribute__((unused))
opencl_get_buffer_id
(
 const void *arg
)
{
  if (arg != NULL) {
    cl_mem buffer = *(cl_mem*)arg;
    return (uint64_t)buffer;
  } else {
    return BUFFER_ID_INVALID;
  }
}


static bool __attribute__((unused))
opencl_isClArgBuffer
(
 const void *arg
)
{
  /*
   * There are 2 scenarios in which opencl_isClArgBuffer will return false
   * 1. When clCreateBuffer was not called for arg before calling clSetKernelArg
   * 2. clEnqueueWriteBuffer is being called for arg. We shouldnt be recording duplicate H2D calls
   * */
  uint64_t buffer_id = opencl_get_buffer_id(arg);
  bool isBuffer;
  if (buffer_id == BUFFER_ID_INVALID) {
    isBuffer = false;
  } else {
    opencl_h2d_map_entry_t *entry = opencl_h2d_map_lookup(buffer_id);
    isBuffer = entry ? true : false;
  }
  //ETMSG(OPENCL, "opencl_isClArgBuffer. buffer_id: %"PRIu64". isBuffer: %d",  buffer_id, isBuffer);
  return isBuffer;
}


static void __attribute__((unused))
add_H2D_metrics_to_cct_node
(
 opencl_h2d_map_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  opencl_object_t *cb_data = opencl_h2d_map_entry_callback_info_get(entry);
  cl_basic_callback_t cb_basic = opencl_cb_basic_get(cb_data);
  opencl_cb_basic_print(cb_basic, "Completion_Callback");

  opencl_clSetKernelArg_activity_process(entry, cb_data);
  opencl_h2d_map_entry_buffer_id_get(entry);
  opencl_h2d_pending_operations_adjust(-1);
}


static void __attribute__((unused))
opencl_update_ccts_for_setClKernelArg
(
 void
)
{
  spinlock_lock(&opencl_h2d_lock);
  opencl_h2d_map_count();
  if (atomic_load(&opencl_h2d_pending_operations) > 0) {
    opencl_update_ccts_for_h2d_nodes(add_H2D_metrics_to_cct_node);
  }
  spinlock_unlock(&opencl_h2d_lock);
}


static void __attribute__((unused))
opencl_wait_for_non_clSetKernelArg_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending h2D operations: %lu", atomic_load(&opencl_h2d_pending_operations));
  while (atomic_load(&opencl_pending_operations) != atomic_load(&opencl_h2d_pending_operations));
}


static void
opencl_wait_for_self_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending self operations: %lu", atomic_load(&opencl_self_pending_operations));
  while (atomic_load(&opencl_self_pending_operations) != 0);
}


static void __attribute__((unused))
opencl_wait_for_all_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending operations: %lu", atomic_load(&opencl_pending_operations));
  while (atomic_load(&opencl_pending_operations) != 0);
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

  FORALL_OPENCL_ROUTINES(OPENCL_BIND);

#undef OPENCL_BIND

  return DYNAMIC_BINDING_STATUS_OK;
#else
  return DYNAMIC_BINDING_STATUS_ERROR;
#endif // ! HPCRUN_STATIC_LINK
}

//******************************************************************************
// interface operations
//******************************************************************************

cl_basic_callback_t
opencl_cb_basic_get
(
 opencl_object_t *cb_data
)
{
  cl_basic_callback_t cb_basic;

  if (cb_data->kind == GPU_ACTIVITY_KERNEL) {
    cb_basic.correlation_id = cb_data->details.ker_cb.correlation_id;
    cb_basic.kind = cb_data->kind;
    cb_basic.type = 0; // not valid

  } else if (cb_data->kind == GPU_ACTIVITY_MEMCPY) {
    cb_basic.correlation_id = cb_data->details.mem_cb.correlation_id;
    cb_basic.kind = cb_data->kind;
    cb_basic.type = cb_data->details.mem_cb.type;
  }
  cb_basic.cct_node = cb_data->details.cct_node;

  return cb_basic;
}


void
opencl_cb_basic_print
(
 cl_basic_callback_t cb_basic,
 char *title
)
{
  ETMSG(OPENCL, " %s | Activity kind: %s | type: %s | correlation id: %"PRIu64 "| cct_node = %p",
        title,
        gpu_kind_to_string(cb_basic.kind),
        gpu_type_to_string(cb_basic.type),
        cb_basic.correlation_id,
        cb_basic.cct_node);
}


void
opencl_subscriber_callback
(
 opencl_object_t *obj
)
{
  // We invoked an opencl operation
  opencl_api_flag = true;

  uint32_t correlation_id = getCorrelationId();

  // Init operations
  atomic_fetch_add(obj->pending_operations, 1);

  gpu_placeholder_type_t placeholder_type;
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;

  switch (obj->kind) {

    case GPU_ACTIVITY_MEMCPY:
      {
        obj->details.cpy_cb.correlation_id = correlation_id;
        if (obj->details.cpy_cb.type == GPU_MEMCPY_H2D){
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyin);

          placeholder_type = gpu_placeholder_type_copyin;

        } else if (obj->details.cpy_cb.type == GPU_MEMCPY_D2H){
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyout);

          placeholder_type = gpu_placeholder_type_copyout;
        }
        break;
      }

    case GPU_ACTIVITY_KERNEL:
      {
        obj->details.ker_cb.correlation_id = correlation_id;
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_kernel);

        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_trace);

        placeholder_type = gpu_placeholder_type_kernel;

        break;
      }

    case GPU_ACTIVITY_MEMORY:
      {
        obj->details.mem_cb.correlation_id = correlation_id;
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_alloc);

        placeholder_type = gpu_placeholder_type_alloc;
        break;
      }

    default:
      assert(0);
  }

  cct_node_t *api_node =
    gpu_application_thread_correlation_callback(correlation_id);
  gpu_op_ccts_t gpu_op_ccts;

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  cct_node_t *cct_ph = gpu_op_ccts_get(&gpu_op_ccts, placeholder_type);
  hpcrun_safe_exit();

  gpu_activity_channel_consume(gpu_metrics_attribute);

  obj->details.cct_node = cct_ph;
  obj->details.initiator_channel = gpu_activity_channel_get();
  obj->details.submit_time = CPU_NANOTIME();

  if (obj->kind == GPU_ACTIVITY_KERNEL && instrumentation) {
    // Callback to produce gtpin correlation
    gtpin_produce_runtime_callstack(&gpu_op_ccts);
  }
}


void
opencl_activity_completion_callback
(
 cl_event event,
 cl_int event_command_exec_status,
 void *user_data
)
{
  opencl_object_t *cb_data = (opencl_object_t*)user_data;
  cl_basic_callback_t cb_basic = opencl_cb_basic_get(cb_data);

  if (event_command_exec_status == CL_COMPLETE) {
    opencl_cb_basic_print(cb_basic, "Completion_Callback");
    opencl_activity_process(event, cb_data, cb_basic.correlation_id);
  }
  if (cb_data->internal_event) {
    HPCRUN_OPENCL_CALL(clReleaseEvent, (event));
  }

  // Finish operations
  opencl_free(cb_data);
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

  gpu_interval_set(interval, (uint64_t)commandStart, (uint64_t)commandEnd);
}


void
opencl_api_initialize
(
 void
)
{
  ETMSG(OPENCL, "CL_TARGET_OPENCL_VERSION: %d", CL_TARGET_OPENCL_VERSION);
  if (instrumentation) {
    gtpin_enable_profiling();
  }
  atomic_store(&correlation_id_counter, 0);
  atomic_store(&opencl_pending_operations, 0);
  atomic_store(&opencl_h2d_pending_operations, 0);
}

//#ifdef ENABLE_GTPIN
#if 1
// one downside of this appproach is that we may override the callback provided by user
cl_int
hpcrun_clBuildProgram
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
  cl_int ret =
    HPCRUN_OPENCL_CALL(clBuildProgram,
		       (program, num_devices, device_list,
			options_with_debug_flags, clBuildProgramCallback,
			user_data));
  free(options_with_debug_flags);
  return ret;
}
#endif // ENABLE_GTPIN


cl_command_queue
hpcrun_clCreateCommandQueue
(
 cl_context context,
 cl_device_id device,
 cl_command_queue_properties properties,
 cl_int *errcode_ret
)
{
  // enabling profiling
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE;

  cl_command_queue queue = HPCRUN_OPENCL_CALL(clCreateCommandQueue, (context, device,
        properties,errcode_ret));

  uint32_t context_id = opencl_cl_context_map_update((uint64_t)context);
  opencl_cl_queue_map_update((uint64_t)queue, context_id);

  return queue;
}


cl_command_queue
hpcrun_clCreateCommandQueueWithProperties
(
 cl_context context,
 cl_device_id device,
 const cl_queue_properties* properties,
 cl_int* errcode_ret
)
{
  cl_queue_properties *queue_properties = (cl_queue_properties *)properties;
  if (properties == NULL) {
    queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * 3);
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
      } else if (properties[props_count] == CL_QUEUE_SIZE) {
        ++props_count;
      }
      ++props_count;
    }

    if (queue_props_id >= 0 && queue_props_id + 1 < props_count) {
      queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * (props_count + 1));
      for (int i = 0; i < props_count; ++i) {
        queue_properties[i] = properties[i];
      }
      // We do have a queue property entry, just enable profiling
      queue_properties[queue_props_id + 1] |= CL_QUEUE_PROFILING_ENABLE;
      queue_properties[props_count] = 0;
    } else {
      // We do not have a queue property entry, need to allocate a queue property entry and set up
      queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * (props_count + 3));
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

  uint32_t context_id = opencl_cl_context_map_update((uint64_t)context);
  opencl_cl_queue_map_update((uint64_t)queue, context_id);
  return queue;
}


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
)
{
  opencl_object_t *kernel_info = opencl_malloc_kind(GPU_ACTIVITY_KERNEL);
  INITIALIZE_CALLBACK_INFO(initializeKernelCallBackInfo, kernel_info, (kernel_info, command_queue))

  opencl_subscriber_callback(kernel_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, kernel_info)

  cl_int return_status =
            HPCRUN_OPENCL_CALL(clEnqueueNDRangeKernel, (command_queue, ocl_kernel, work_dim,
                                global_work_offset, global_work_size, local_work_size,
                                num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind: Kernel. "
                "Correlation id: %"PRIu64 "", kernel_info->details.ker_cb.correlation_id);

  HPCRUN_OPENCL_CALL
    (clSetEventCallback,
     (*eventp, CL_COMPLETE, &opencl_activity_completion_callback, kernel_info));

  return return_status;
}


// this is a simplified version of clEnqueueNDRangeKernel, TODO: check if code duplication can be avoided
cl_int
hpcrun_clEnqueueTask
(
 cl_command_queue command_queue,
 cl_kernel kernel,
 cl_uint num_events_in_wait_list,
 const cl_event* event_wait_list,
 cl_event* event
)
{
  opencl_object_t *kernel_info = opencl_malloc_kind(GPU_ACTIVITY_KERNEL);
  INITIALIZE_CALLBACK_INFO(initializeKernelCallBackInfo, kernel_info, (kernel_info, command_queue))

  opencl_subscriber_callback(kernel_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, kernel_info);

  cl_int return_status =
            HPCRUN_OPENCL_CALL(clEnqueueTask, (command_queue, kernel,
                                num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind: Kernel. "
                "Correlation id: %"PRIu64 "", kernel_info->details.ker_cb.correlation_id);

  HPCRUN_OPENCL_CALL
    (clSetEventCallback,
     (*eventp, CL_COMPLETE, &opencl_activity_completion_callback, kernel_info));

  return return_status;
}


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
)
{
  ETMSG(OPENCL, "inside clEnqueueReadBuffer wrapper");

  opencl_object_t *cpy_info = opencl_malloc_kind(GPU_ACTIVITY_MEMCPY);
  INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_D2H, cb, command_queue))

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  cl_int return_status =
    HPCRUN_OPENCL_CALL(clEnqueueReadBuffer,
      (command_queue, buffer, blocking_read, offset,
       cb, ptr, num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
        (long)cb);


  HPCRUN_OPENCL_CALL
    (clSetEventCallback,
     (*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info));

  return return_status;
}


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
)
{
  ETMSG(OPENCL, "inside clEnqueueWriteBuffer wrapper. cl_mem buffer: %p", buffer);
  opencl_object_t *cpy_info = opencl_malloc_kind(GPU_ACTIVITY_MEMCPY);
  INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_H2D, cb, command_queue))

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  cl_int return_status =
  HPCRUN_OPENCL_CALL(clEnqueueWriteBuffer,
                     (command_queue, buffer, blocking_write, offset, cb, ptr,
                          num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
        (long)cb);

  HPCRUN_OPENCL_CALL
    (clSetEventCallback,
     (*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info));

  return return_status;
}


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
)
{
  ETMSG(OPENCL, "inside clEnqueueMapBuffer wrapper");

  opencl_object_t *cpy_info = opencl_malloc_kind(GPU_ACTIVITY_MEMCPY);
  if (map_flags == CL_MAP_READ) {
    INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_D2H, size, command_queue));
  } else {
    //map_flags == CL_MAP_WRITE || map_flags == CL_MAP_WRITE_INVALIDATE_REGION
    INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_H2D, size, command_queue));
  }

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  void *map_ptr =
  HPCRUN_OPENCL_CALL(clEnqueueMapBuffer,
                     (command_queue, buffer, blocking_map, map_flags, offset,
                     size, num_events_in_wait_list, event_wait_list, eventp, errcode_ret));

  if (map_flags == CL_MAP_READ) {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                  "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
          (long)size);
  } else {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                  "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
          (long)size);
  }

  HPCRUN_OPENCL_CALL
    (clSetEventCallback,
     (*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info));

  return map_ptr;
}


cl_mem
hpcrun_clCreateBuffer
(
 cl_context context,
 cl_mem_flags flags,
 size_t size,
 void* host_ptr,
 cl_int* errcode_ret
)
{
  ETMSG(OPENCL, "clCreateBuffer flags: %u, size: %"PRIu64 "", flags, size);

  opencl_object_t *mem_info = opencl_malloc_kind(GPU_ACTIVITY_MEMORY);
  INITIALIZE_CALLBACK_INFO(initializeMemoryCallBackInfo, mem_info, (mem_info, flags, size))

  opencl_subscriber_callback(mem_info);

  gpu_interval_t interval;
  interval.start = CPU_NANOTIME();
  cl_mem buffer =
    HPCRUN_OPENCL_CALL(clCreateBuffer, (context, flags, size, host_ptr, errcode_ret));
  interval.end = CPU_NANOTIME();

  opencl_operation_multiplexer_push(interval, mem_info, mem_info->details.mem_cb.correlation_id);

  opencl_free(mem_info);

  return buffer;
}


void
opencl_instrumentation_enable
(
 void
)
{
  instrumentation = true;
}


void
opencl_api_thread_finalize
(
 void *args,
 int how
)
{
  if (opencl_api_flag) {
    // If I have invoked any opencl api, I have to attribute all my activities to my ccts
    opencl_api_flag = false;

    atomic_bool wait;
    atomic_store(&wait, true);
    gpu_activity_t gpu_activity;
    memset(&gpu_activity, 0, sizeof(gpu_activity_t));

    gpu_activity.kind = GPU_ACTIVITY_FLUSH;
    gpu_activity.details.flush.wait = &wait;
    gpu_operation_multiplexer_push(gpu_activity_channel_get(), NULL, &gpu_activity);

    // Wait until operations are drained
    // Operation channel is FIFO
    while (atomic_load(&wait)) {}

    // Wait until my activities are drained
    if (how == MONITOR_EXIT_NORMAL) opencl_wait_for_self_pending_operations();

    // Now I can attribute activities
    gpu_application_thread_process_activities();
  }
}


void
opencl_api_process_finalize
(
 void *args,
 int how
)
{
  gpu_operation_multiplexer_fini();
}
