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
#include <hpcrun/messages/messages.h>
#include <hpcrun/sample-sources/libdl.h>
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

#define FORALL_OPENCL_ERRORS(macro)					\
  macro(CL_SUCCESS)							\
  macro(CL_DEVICE_NOT_FOUND)						\
  macro(CL_DEVICE_NOT_AVAILABLE)					\
  macro(CL_COMPILER_NOT_AVAILABLE)					\
  macro(CL_MEM_OBJECT_ALLOCATION_FAILURE)				\
  macro(CL_OUT_OF_RESOURCES)						\
  macro(CL_OUT_OF_HOST_MEMORY)						\
  macro(CL_PROFILING_INFO_NOT_AVAILABLE)				\
  macro(CL_MEM_COPY_OVERLAP)						\
  macro(CL_IMAGE_FORMAT_MISMATCH)					\
  macro(CL_IMAGE_FORMAT_NOT_SUPPORTED)					\
  macro(CL_BUILD_PROGRAM_FAILURE)					\
  macro(CL_MAP_FAILURE)							\
  macro(CL_MISALIGNED_SUB_BUFFER_OFFSET)				\
  macro(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST)			\
  macro(CL_COMPILE_PROGRAM_FAILURE)					\
  macro(CL_LINKER_NOT_AVAILABLE)					\
  macro(CL_LINK_PROGRAM_FAILURE)					\
  macro(CL_DEVICE_PARTITION_FAILED)					\
  macro(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)				\
  macro(CL_INVALID_VALUE)						\
  macro(CL_INVALID_DEVICE_TYPE)						\
  macro(CL_INVALID_PLATFORM)						\
  macro(CL_INVALID_DEVICE)						\
  macro(CL_INVALID_CONTEXT)						\
  macro(CL_INVALID_QUEUE_PROPERTIES)					\
  macro(CL_INVALID_COMMAND_QUEUE)					\
  macro(CL_INVALID_HOST_PTR)						\
  macro(CL_INVALID_MEM_OBJECT)						\
  macro(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)				\
  macro(CL_INVALID_IMAGE_SIZE)						\
  macro(CL_INVALID_SAMPLER)						\
  macro(CL_INVALID_BINARY)						\
  macro(CL_INVALID_BUILD_OPTIONS)					\
  macro(CL_INVALID_PROGRAM)						\
  macro(CL_INVALID_PROGRAM_EXECUTABLE)					\
  macro(CL_INVALID_KERNEL_NAME)						\
  macro(CL_INVALID_KERNEL_DEFINITION)					\
  macro(CL_INVALID_KERNEL)						\
  macro(CL_INVALID_ARG_INDEX)						\
  macro(CL_INVALID_ARG_VALUE)						\
  macro(CL_INVALID_ARG_SIZE)						\
  macro(CL_INVALID_KERNEL_ARGS)						\
  macro(CL_INVALID_WORK_DIMENSION)					\
  macro(CL_INVALID_WORK_GROUP_SIZE)					\
  macro(CL_INVALID_WORK_ITEM_SIZE)					\
  macro(CL_INVALID_GLOBAL_OFFSET)					\
  macro(CL_INVALID_EVENT_WAIT_LIST)					\
  macro(CL_INVALID_EVENT)						\
  macro(CL_INVALID_OPERATION)						\
  macro(CL_INVALID_GL_OBJECT)						\
  macro(CL_INVALID_BUFFER_SIZE)						\
  macro(CL_INVALID_MIP_LEVEL)						\
  macro(CL_INVALID_GLOBAL_WORK_SIZE)					\
  macro(CL_INVALID_PROPERTY)						\
  macro(CL_INVALID_IMAGE_DESCRIPTOR)					\
  macro(CL_INVALID_COMPILER_OPTIONS)					\
  macro(CL_INVALID_LINKER_OPTIONS)					\
  macro(CL_INVALID_DEVICE_PARTITION_COUNT)


#define CODE_TO_STRING(e) case e: return #e;

#define opencl_path() "libOpenCL.so"

#define FORALL_OPENCL_ROUTINES(macro)					\
  macro(clGetEventProfilingInfo)					\
  macro(clReleaseEvent)							\
  macro(clSetEventCallback)

#define OPENCL_FN_NAME(f) DYN_FN_NAME(f)

#define OPENCL_FN(fn, args)			\
  static cl_int (*OPENCL_FN_NAME(fn)) args

#define HPCRUN_OPENCL_CALL(fn, args)					\
  {									\
    cl_int status = OPENCL_FN_NAME(fn) args;				\
    if (status != CL_SUCCESS) {						\
      ETMSG(OPENCL, "opencl call failed: %s",				\
	    opencl_error_report(status));				\
    }									\
  }



//******************************************************************************
// local data
//******************************************************************************

static atomic_long correlation_id_counter;

//----------------------------------------------------------
// opencl function pointers for late binding
//----------------------------------------------------------

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
  opencl_activity_translate(&gpu_activity, event, cb_data);

  if (gpu_activity_is_multiplexer_initialized() == false){
    gpu_activity_multiplexer_init();
  }
  gpu_activity_multiplexer_push(cb_data->details.initiator_channel, &gpu_activity);
//  gpu_activity_process(&gpu_activity);
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

  return cb_basic;
}

void
opencl_cb_basic_print
(
 cl_basic_callback_t cb_basic,
 char *title
)
{

  ETMSG(OPENCL, " %s | Activity kind: %s | type: %s | correlation id: %"PRIu64 "",
        title,
        gpu_kind_to_string(cb_basic.kind),
        gpu_type_to_string(cb_basic.type),
        cb_basic.correlation_id);

}


void
opencl_initialize_correlation_id
(
 void
)
{
  atomic_store(&correlation_id_counter, 0);
}

void
opencl_subscriber_callback
(
  opencl_object_t *cb_info
)
{

  uint64_t correlation_id = getCorrelationId();

  opencl_pending_operations_adjust(1);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_ccts_t gpu_op_ccts;
  gpu_correlation_id_map_insert(correlation_id, correlation_id);
  cct_node_t *api_node = 
    gpu_application_thread_correlation_callback(correlation_id);


  switch (cb_info->kind) {

    case GPU_ACTIVITY_MEMCPY:
      cb_info->details.mem_cb.correlation_id = correlation_id;
      if (cb_info->details.mem_cb.type == GPU_MEMCPY_H2D){
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                       gpu_placeholder_type_copyin);

      }else if (cb_info->details.mem_cb.type == GPU_MEMCPY_D2H){
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                       gpu_placeholder_type_copyout);
      }
      break;

    case GPU_ACTIVITY_KERNEL:
      cb_info->details.ker_cb.correlation_id = correlation_id;
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

  gpu_activity_channel_consume(gpu_metrics_attribute);


  cb_info->details.cct_node = api_node;
  cb_info->details.initiator_channel = gpu_activity_channel_get();
  cb_info->details.submit_time = CPU_NANOTIME();

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
getTimingInfoFromClEvent
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
  opencl_intercept_setup();
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
  
#define OPENCL_BIND(fn)				\
  CHK_DLSYM(opencl, fn);
  
  FORALL_OPENCL_ROUTINES(OPENCL_BIND)
    
#undef OPENCL_BIND
    
    return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK  
}


void
opencl_api_finalize
(
  void *args
)
{
  opencl_wait_for_pending_operations();
  gpu_activity_multiplexer_fini();

  gpu_application_thread_process_activities();

}
