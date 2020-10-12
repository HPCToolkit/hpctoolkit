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
#include <hpcrun/gpu/gpu-activity-multiplexer.h>
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
#include "opencl-context-map.h"
#include "opencl-kernel-map.h"



//******************************************************************************
// macros
//******************************************************************************

#define CPU_NANOTIME() (usec_time() * 1000)

#define opencl_path() "libOpenCL.so"

#define FORALL_OPENCL_ROUTINES(macro)  \
  macro(clBuildProgram)  \
  macro(clCreateProgramWithSource)  \
  macro(clCreateCommandQueue)  \
  macro(clCreateCommandQueueWithProperties)  \
  macro(clEnqueueNDRangeKernel)  \
  macro(clEnqueueReadBuffer)  \
  macro(clEnqueueWriteBuffer)  \
  macro(clEnqueueMapBuffer) \
  macro(clCreateBuffer)  \
  macro(clSetKernelArg)  \
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



//******************************************************************************
// local data
//******************************************************************************

static atomic_long correlation_id_counter;
static atomic_ullong opencl_pending_operations;
static atomic_ullong opencl_h2d_pending_operations;
static spinlock_t opencl_h2d_lock = SPINLOCK_UNLOCKED;
static bool instrumentation = false;

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100



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
  clSetKernelArg,
  (
    cl_kernel kernel,
    cl_uint arg_index,
    size_t arg_size,
    const void* arg_value
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

static uint64_t
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
 uint64_t correlation_id
)
{
  ker_info->kind = GPU_ACTIVITY_KERNEL;
  ker_info->details.ker_cb.correlation_id = correlation_id;
}


static void
initializeMemoryCallBackInfo
(
  opencl_object_t *mem_info,
  gpu_memcpy_type_t type,
  size_t size,
  uint64_t correlation_id
)
{
  mem_info->kind = GPU_ACTIVITY_MEMCPY;
  mem_info->details.mem_cb.type = type;
  mem_info->details.mem_cb.fromHostToDevice = (type == GPU_MEMCPY_H2D);
  mem_info->details.mem_cb.fromDeviceToHost = (type == GPU_MEMCPY_D2H);
  mem_info->details.mem_cb.size = size;

  mem_info->details.mem_cb.correlation_id = correlation_id;
}


static void
initializeClSetKernelArgMemoryCallBackInfo
(
  opencl_object_t *mem_info,
  gpu_memcpy_type_t type,
  size_t size,
  uint64_t correlation_id,
  uint32_t context_id,
  uint32_t stream_id
)
{
  mem_info->kind = GPU_ACTIVITY_MEMCPY;
  mem_info->details.mem_cb.type = type;
  mem_info->details.mem_cb.fromHostToDevice = (type == GPU_MEMCPY_H2D);
  mem_info->details.mem_cb.fromDeviceToHost = (type == GPU_MEMCPY_D2H);
  mem_info->details.mem_cb.size = size;

  mem_info->details.mem_cb.correlation_id = correlation_id;
  mem_info->details.context_id = context_id;
  mem_info->details.stream_id = stream_id;
}


static void opencl_activity_completion_notify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
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
opencl_h2d_pending_operations_adjust
(
 int value
)
{
  atomic_fetch_add(&opencl_h2d_pending_operations, value);
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
opencl_activity_process
(
  cl_event event,
  opencl_object_t *cb_data
)
{
  gpu_activity_t gpu_activity;

  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));
  opencl_timing_info_get(&interval, event);
  
  opencl_activity_translate(&gpu_activity, cb_data, interval);

  if (gpu_activity_multiplexer_my_channel_initialized() == false){
    gpu_activity_multiplexer_my_channel_init();
  }
  gpu_activity_multiplexer_push(cb_data->details.initiator_channel, &gpu_activity);
}


static void
opencl_clSetKernelArg_activity_process
(
  opencl_h2d_map_entry_t *entry,
  opencl_object_t *cb_data
)
{
  gpu_activity_t gpu_activity;
  uint64_t correlation_id = opencl_h2d_map_entry_correlation_get(entry);
	size_t size = opencl_h2d_map_entry_size_get(entry); 
  cb_data->details.ker_cb.correlation_id = correlation_id;

  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));

  opencl_activity_translate(&gpu_activity, cb_data, interval);
  
  if (gpu_activity_multiplexer_my_channel_initialized() == false){
    gpu_activity_multiplexer_my_channel_init();
  }
  ETMSG(OPENCL, "cb_data->details.initiator_channel: %p", cb_data->details.initiator_channel);
  gpu_activity_multiplexer_push(cb_data->details.initiator_channel, &gpu_activity);
  //gpu_activity_process(&gpu_activity);
}


static uint64_t
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


static bool
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
	//ETMSG(OPENCL, "opencl_isClArgBuffer. buffer_id: %"PRIu64". isBuffer: %d",	buffer_id, isBuffer);
	return isBuffer;
}


static void
add_H2D_metrics_to_cct_node
(
	opencl_h2d_map_entry_t *entry,
	splay_visit_t visit_type,
	void *arg
)
{
	// uint64_t correlation_id = opencl_h2d_map_entry_correlation_get(entry); 
	// gpu_correlation_id_map_entry_t *cid_map_entry = 
	// 	gpu_correlation_id_map_lookup(correlation_id);
	// if (cid_map_entry == NULL) {
	// 	ETMSG(OPENCL, "cid_map_entry for correlation_id: %"PRIu64 " (clSetKernelArg H2D) not found", correlation_id);
	// 	return;
	// }

	//opencl_activity_completion_notify();
  opencl_object_t *cb_data = opencl_h2d_map_entry_callback_info_get(entry);
  cl_basic_callback_t cb_basic = opencl_cb_basic_get(cb_data);
  opencl_cb_basic_print(cb_basic, "Completion_Callback");

	opencl_clSetKernelArg_activity_process(entry, cb_data);
	uint64_t buffer_id = opencl_h2d_map_entry_buffer_id_get(entry);
	//opencl_h2d_map_delete(buffer_id);
  opencl_h2d_pending_operations_adjust(-1);
  opencl_pending_operations_adjust(-1);
}


static void
opencl_update_ccts_for_setClKernelArg
(
	void
)
{
  spinlock_lock(&opencl_h2d_lock);
  uint64_t count = opencl_h2d_map_count();
	if (atomic_load(&opencl_h2d_pending_operations) > 0) {
		opencl_update_ccts_for_h2d_nodes(add_H2D_metrics_to_cct_node);
	}
  spinlock_unlock(&opencl_h2d_lock);
}


static void
opencl_wait_for_non_clSetKernelArg_pending_operations
(
  void
)
{
  ETMSG(OPENCL, "pending h2D operations: %lu", atomic_load(&opencl_h2d_pending_operations));
  while (atomic_load(&opencl_pending_operations) != atomic_load(&opencl_h2d_pending_operations));
}


static void
opencl_wait_for_all_pending_operations
(
  void
)
{
  ETMSG(OPENCL, "pending operations: %lu", atomic_load(&opencl_pending_operations));
  while (atomic_load(&opencl_pending_operations) != 0);
}


static bool
opencl_in_correlation_map(cl_basic_callback_t cb_basic){
  gpu_correlation_id_map_entry_t *cid_map_entry = gpu_correlation_id_map_lookup(cb_basic.correlation_id);
  if (cid_map_entry == NULL) {
    ETMSG(OPENCL, "Activity not in correlation map \n");
    opencl_cb_basic_print(cb_basic, "NOT in Correlation map");
    return false;
  }
  return true;
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


static uint64_t
get_corr_id
(
 opencl_object_t *cb_info
){
  return cb_info->details.ker_cb.correlation_id;
}



void
opencl_subscriber_callback
(
  opencl_object_t *cb_info
)
{
  gpu_placeholder_type_t placeholder_type;
  uint64_t correlation_id;

  if( get_corr_id(cb_info) == CORRELATION_ID_INVALID){
    correlation_id = getCorrelationId();
  }else{
    correlation_id = get_corr_id(cb_info);
  }

  opencl_pending_operations_adjust(1);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_correlation_id_map_insert(correlation_id, correlation_id);

  switch (cb_info->kind) {

    case GPU_ACTIVITY_MEMCPY:
      cb_info->details.mem_cb.correlation_id = correlation_id;
      if (cb_info->details.mem_cb.type == GPU_MEMCPY_H2D){ 
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                       gpu_placeholder_type_copyin);

        placeholder_type = gpu_placeholder_type_copyin;

      }else if (cb_info->details.mem_cb.type == GPU_MEMCPY_D2H){
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                       gpu_placeholder_type_copyout);

        placeholder_type = gpu_placeholder_type_copyout;
      }
      break;

    case GPU_ACTIVITY_KERNEL:
      cb_info->details.ker_cb.correlation_id = correlation_id;
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
           gpu_placeholder_type_kernel);

      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				   gpu_placeholder_type_trace);

      placeholder_type = gpu_placeholder_type_kernel;

      break;
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

  cb_info->details.cct_node = cct_ph;
  cb_info->details.initiator_channel = gpu_activity_channel_get();
  cb_info->details.submit_time = CPU_NANOTIME();

  if (cb_info->kind == GPU_ACTIVITY_KERNEL && instrumentation) {
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
    opencl_in_correlation_map(cb_basic);

    opencl_cb_basic_print(cb_basic, "Completion_Callback");
    opencl_activity_process(event, cb_data);
  }
  if (cb_data->isInternalClEvent) {
    HPCRUN_OPENCL_CALL(clReleaseEvent, (event));
  }
  opencl_free(cb_data);
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
  ETMSG(OPENCL, "CL_TARGET_OPENCL_VERSION: %d", CL_TARGET_OPENCL_VERSION);
  if (instrumentation) {
    gpu_metrics_GPU_INST_enable();
    gtpin_enable_profiling();
  }
  atomic_store(&correlation_id_counter, 0);
  atomic_store(&opencl_pending_operations, 0);
  atomic_store(&opencl_h2d_pending_operations, 0);
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

  if (strings != NULL && lengths != NULL) {
    FILE *f_ptr;
    for (int i = 0; i < (int)count; i++) {
      // what if a single file has multiple kernels?
      // we need to add logic to get filenames by reading the strings contents
      char fileno = '0' + (i + 1); // right now we are naming the files as index numbers
      // using malloc instead of hpcrun_malloc gives extra garbage characters in file name
      char *filename = (char *)hpcrun_malloc(sizeof(fileno) + 1);
      *filename = fileno + '\0';
      f_ptr = fopen(filename, "w");
      fwrite(strings[i], lengths[i], 1, f_ptr);
    }
    fclose(f_ptr);
  }

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

  cl_command_queue queue = HPCRUN_OPENCL_CALL(clCreateCommandQueue, (context, device,
        properties,errcode_ret));
  opencl_cl_context_map_insert((uint64_t)context, (uint32_t)queue);
  return queue;
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
        // TODO(Keren): A temporay hack
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
  opencl_cl_context_map_insert((uint64_t)context, (uint32_t)queue);
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
  opencl_object_t *kernel_info = opencl_malloc();
  initializeKernelCallBackInfo(kernel_info, CORRELATION_ID_INVALID);

  opencl_subscriber_callback(kernel_info);

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
                                global_work_offset, global_work_size, local_work_size,
                                num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind: Kernel. "
                "Correlation id: %"PRIu64 "", kernel_info->details.ker_cb.correlation_id);

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
  ETMSG(OPENCL, "inside clEnqueueReadBuffer wrapper");

  opencl_object_t *mem_info = opencl_malloc();
  initializeMemoryCallBackInfo(mem_info, GPU_MEMCPY_D2H, cb, CORRELATION_ID_INVALID);
  opencl_subscriber_callback(mem_info);

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
  HPCRUN_OPENCL_CALL(clEnqueueReadBuffer,
                     (command_queue, buffer, blocking_read, offset,
                     cb, ptr, num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                "Correlation id: %"PRIu64 "", mem_info->details.mem_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
        (long)cb);


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
  ETMSG(OPENCL, "inside clEnqueueWriteBuffer wrapper. cl_mem buffer: %p", buffer);
  opencl_h2d_map_delete((uint64_t)buffer);
	opencl_h2d_pending_operations_adjust(-1);
  //opencl_pending_operations_adjust(-1);
  opencl_object_t *mem_info = opencl_malloc();
  initializeMemoryCallBackInfo(mem_info, GPU_MEMCPY_H2D, cb, CORRELATION_ID_INVALID);
  opencl_subscriber_callback(mem_info);

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
  HPCRUN_OPENCL_CALL(clEnqueueWriteBuffer,
                     (command_queue, buffer, blocking_write, offset, cb, ptr,
                          num_events_in_wait_list, event_wait_list, eventp));

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                "Correlation id: %"PRIu64 "", mem_info->details.mem_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
        (long)cb);

  clSetEventCallback_wrapper(*eventp, CL_COMPLETE,
                             &opencl_activity_completion_callback,
                             (void*) mem_info);

  return return_status;
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
  ETMSG(OPENCL, "inside clEnqueueMapBuffer wrapper");

  opencl_object_t *mem_info = opencl_malloc();
  if (map_flags == CL_MAP_READ) {
    initializeMemoryCallBackInfo(mem_info, GPU_MEMCPY_D2H, size, CORRELATION_ID_INVALID);
  } else {
    //map_flags == CL_MAP_WRITE || map_flags == CL_MAP_WRITE_INVALIDATE_REGION
    initializeMemoryCallBackInfo(mem_info, GPU_MEMCPY_H2D, size, CORRELATION_ID_INVALID);
  }
  
  opencl_subscriber_callback(mem_info);

  cl_event my_event;
  cl_event *eventp;
  if (!event) {
    mem_info->isInternalClEvent = true;
    eventp = &my_event;
  } else {
    eventp = event;
    mem_info->isInternalClEvent = false;
  }

  void *map_ptr =
  HPCRUN_OPENCL_CALL(clEnqueueMapBuffer,
                     (command_queue, buffer, blocking_map, map_flags, offset,
                     size, num_events_in_wait_list, event_wait_list, eventp, errcode_ret));

  if (map_flags == CL_MAP_READ) {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                  "Correlation id: %"PRIu64 "", mem_info->details.mem_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
          (long)size);
  } else {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                  "Correlation id: %"PRIu64 "", mem_info->details.mem_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
          (long)size);
  }


  clSetEventCallback_wrapper(*eventp, CL_COMPLETE,
                             &opencl_activity_completion_callback, mem_info);

  return map_ptr;
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
	uint64_t correlation_id = getCorrelationId();
	opencl_h2d_pending_operations_adjust(1);
  cl_mem buffer = 
    HPCRUN_OPENCL_CALL(clCreateBuffer, (context, flags, size, host_ptr, errcode_ret));
  uint64_t buffer_id = (uint64_t)buffer; 
  //ETMSG(OPENCL, "inside clCreateBuffer wrapper. cl_mem buffer: %p. buffer_id: %"PRIu64"", buffer, buffer_id);
	opencl_h2d_map_insert(buffer_id, correlation_id, size, NULL);
  
  return buffer;
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
	bool isClBuffer = opencl_isClArgBuffer(arg_value);
  //ETMSG(OPENCL, "inside clSetKernelArg wrapper."); //isClBuffer: %d. *(cl_mem*)arg_value: %p",isClBuffer, *(cl_mem*)arg_value

  cl_int return_status = 
    HPCRUN_OPENCL_CALL(clSetKernelArg, (kernel, arg_index, arg_size, arg_value));
	if (!isClBuffer) {
		return return_status;	
	}

  size_t context_size;
  cl_int STATUS = clGetKernelInfo(kernel, CL_KERNEL_CONTEXT, 0, NULL, &context_size);
  cl_context *context = malloc(sizeof(context_size));
  STATUS = clGetKernelInfo(kernel, CL_KERNEL_CONTEXT, context_size, (void*)context, NULL);
  
  //opencl_cl_kernel_map_insert((uint64_t)ocl_kernel, (uint32_t)(*context), command_queue);

  uint32_t context_id = (uint32_t)(*context);
  opencl_context_map_entry_t *ce = opencl_cl_context_map_lookup((uint64_t)(*context));
  uint32_t stream_id = opencl_cl_kernel_map_entry_stream_get(ce);

  uint64_t buffer_id = opencl_get_buffer_id(arg_value);
	opencl_h2d_map_entry_t *entry = opencl_h2d_map_lookup(buffer_id);
	if (entry) {
		size_t size = opencl_h2d_map_entry_size_get(entry);

		uint64_t correlation_id = opencl_h2d_map_entry_correlation_get(entry);
    opencl_object_t *mem_info = opencl_malloc();
    initializeClSetKernelArgMemoryCallBackInfo(mem_info, GPU_MEMCPY_H2D, size, correlation_id, context_id, stream_id);
    opencl_subscriber_callback(mem_info);

    /* There is no way to record start_time, end_time for the memory transfer that happens as part of clSetKernelArg
      This is because clSetKernelArg sets argument for a kernel in a context. But in a context, there can be multiple
      device-queue pairs and opencl does not provide events or listeners to the queue so that we can read the memory operations.
      Since the memory transfer is async, there are no event handles and we dont know which device is the receiver;
      the timing information cannot be calculated.
    */
  	opencl_h2d_map_insert(buffer_id,correlation_id, size, mem_info);
	}
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
opencl_api_thread_finalize
(
 void *args
)
{
	opencl_wait_for_non_clSetKernelArg_pending_operations();
	opencl_update_ccts_for_setClKernelArg();
  opencl_wait_for_all_pending_operations();
  gpu_application_thread_process_activities();
}


void
opencl_api_process_finalize
(
void *args
)
{
  opencl_api_thread_finalize(NULL);
  gpu_activity_multiplexer_fini();
}
