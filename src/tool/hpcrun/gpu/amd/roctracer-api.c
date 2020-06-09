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
// local includes
//******************************************************************************

#include "roctracer-api.h"
#include "roctracer-activity-translate.h"

#include <roctracer_hip.h>

#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/libdl.h>

#include <hpcrun/utilities/hpcrun-nanotime.h>



//******************************************************************************
// macros
//******************************************************************************

#define FORALL_ROCTRACER_ROUTINES(macro)			\
  macro(roctracer_open_pool_expl)   \
  macro(roctracer_enable_callback)  \
  macro(roctracer_enable_activity_expl)  \
  macro(roctracer_disable_callback) \
  macro(roctracer_disable_activity) \
  macro(roctracer_flush_activity_expl)   \
  macro(roctracer_activity_push_external_correlation_id) \
  macro(roctracer_activity_pop_external_correlation_id)


#define ROCTRACER_FN_NAME(f) DYN_FN_NAME(f)

#define ROCTRACER_FN(fn, args) \
  static roctracer_status_t (*ROCTRACER_FN_NAME(fn)) args

#define HPCRUN_ROCTRACER_CALL(fn, args) \
{      \
  roctracer_status_t status = ROCTRACER_FN_NAME(fn) args;	\
  if (status != ROCTRACER_STATUS_SUCCESS) {		\
    /* use roctracer_error_string() */ \
  }						\
}



//******************************************************************************
// local variables
//******************************************************************************

//----------------------------------------------------------
// roctracer function pointers for late binding
//----------------------------------------------------------

ROCTRACER_FN
(
 roctracer_open_pool_expl,
 (
  const roctracer_properties_t*,
  roctracer_pool_t**
 )
);

ROCTRACER_FN
(
 roctracer_enable_callback,
 (
  activity_rtapi_callback_t,
  void*
 )
);

ROCTRACER_FN
(
 roctracer_enable_activity_expl,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_disable_callback,
 (
  void
 )
);

ROCTRACER_FN
(
 roctracer_disable_activity,
 (
  void
 )
);

ROCTRACER_FN
(
 roctracer_flush_activity_expl,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_activity_push_external_correlation_id,
 (
  activity_correlation_id_t
 )
);

ROCTRACER_FN
(
 roctracer_activity_pop_external_correlation_id,
 (
  activity_correlation_id_t*
 )
);



//******************************************************************************
// private operations
//******************************************************************************

#if 0
static void
roctracer_correlation_id_push
(
 uint64_t id
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_push_external_correlation_id, (id));
}


static void
roctracer_correlation_id_pop
(
 uint64_t* id
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_pop_external_correlation_id, (id));
}
#endif


#if 0
static void
roctracer_kernel_data_set
(
 const hip_api_data_t *data,
 entry_data_t *entry_data,
 uint32_t callback_id
)
{
  switch(callback_id)
    {
    case HIP_API_ID_hipModuleLaunchKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipModuleLaunchKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	data->args.hipModuleLaunchKernel.blockDimX *
	data->args.hipModuleLaunchKernel.blockDimY *
	data->args.hipModuleLaunchKernel.blockDimZ;
      break;

    case HIP_API_ID_hipLaunchCooperativeKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipLaunchCooperativeKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	data->args.hipLaunchCooperativeKernel.blockDimX.x *
	data->args.hipLaunchCooperativeKernel.blockDimX.y *
	data->args.hipLaunchCooperativeKernel.blockDimX.z;
      break;

    case HIP_API_ID_hipHccModuleLaunchKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipHccModuleLaunchKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	(data->args.hipHccModuleLaunchKernel.globalWorkSizeX *
	 data->args.hipHccModuleLaunchKernel.globalWorkSizeY *
	 data->args.hipHccModuleLaunchKernel.globalWorkSizeZ) +
	(data->args.hipHccModuleLaunchKernel.localWorkSizeX *
	 data->args.hipHccModuleLaunchKernel.localWorkSizeY *
	 data->args.hipHccModuleLaunchKernel.localWorkSizeZ);
      break;
    }
}
#endif



static void
roctracer_subscriber_callback
(
 uint32_t domain,
 uint32_t callback_id,
 const void* callback_data,
 void* arg
)
{
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  bool is_valid_op = false;
  const hip_api_data_t* data = (const hip_api_data_t*)(callback_data);

  switch (callback_id) {
  case HIP_API_ID_hipMemcpy:
  case HIP_API_ID_hipMemcpyToSymbolAsync:
  case HIP_API_ID_hipMemcpyFromSymbolAsync:
  case HIP_API_ID_hipMemcpyDtoD:
  case HIP_API_ID_hipMemcpy2DToArray:
  case HIP_API_ID_hipMemcpyAsync:
  case HIP_API_ID_hipMemcpyFromSymbol:
  case HIP_API_ID_hipMemcpy3D:
  case HIP_API_ID_hipMemcpyAtoH:
  case HIP_API_ID_hipMemcpyHtoD:
  case HIP_API_ID_hipMemcpyHtoA:
  case HIP_API_ID_hipMemcpy2D:
  case HIP_API_ID_hipMemcpyPeerAsync:
  case HIP_API_ID_hipMemcpyDtoH:
  case HIP_API_ID_hipMemcpyHtoDAsync:
  case HIP_API_ID_hipMemcpyFromArray:
  case HIP_API_ID_hipMemcpy2DAsync:
  case HIP_API_ID_hipMemcpyToArray:
  case HIP_API_ID_hipMemcpyToSymbol:
  case HIP_API_ID_hipMemcpyPeer:
  case HIP_API_ID_hipMemcpyDtoDAsync:
  case HIP_API_ID_hipMemcpyDtoHAsync:
  case HIP_API_ID_hipMemcpyParam2D:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_copy);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipMalloc:
  case HIP_API_ID_hipMallocPitch:
  case HIP_API_ID_hipMalloc3DArray:
  case HIP_API_ID_hipMallocArray:
  case HIP_API_ID_hipHostMalloc:
  case HIP_API_ID_hipMallocManaged:
  case HIP_API_ID_hipMalloc3D:
  case HIP_API_ID_hipExtMallocWithFlags:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_alloc);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipMemset3DAsync:
  case HIP_API_ID_hipMemset2D:
  case HIP_API_ID_hipMemset2DAsync:
  case HIP_API_ID_hipMemset:
  case HIP_API_ID_hipMemsetD8:
  case HIP_API_ID_hipMemset3D:
  case HIP_API_ID_hipMemsetAsync:
  case HIP_API_ID_hipMemsetD32Async:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_memset);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipFree:
  case HIP_API_ID_hipFreeArray:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_delete);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipModuleLaunchKernel:
  case HIP_API_ID_hipLaunchCooperativeKernel:
  case HIP_API_ID_hipHccModuleLaunchKernel:
    //case HIP_API_ID_hipExtModuleLaunchKernel:

    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_kernel);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipCtxSynchronize:
  case HIP_API_ID_hipStreamSynchronize:
  case HIP_API_ID_hipDeviceSynchronize:
  case HIP_API_ID_hipEventSynchronize:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_sync);
    is_valid_op = true;
    break;

  default:
    break;
  }

  if (!is_valid_op) return;


  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    uint64_t correlation_id = data->correlation_id;
    cct_node_t *api_node =
      gpu_application_thread_correlation_callback(correlation_id);

    gpu_op_ccts_t gpu_op_ccts;
    hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
    hpcrun_safe_exit();

    gpu_activity_channel_consume(gpu_metrics_attribute);

    // Generate notification entry
    uint64_t cpu_submit_time = hpcrun_nanotime();
    gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
  }
}


static void
roctracer_buffer_completion_notify
(
  void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
roctracer_activity_process
(
 roctracer_record_t* roctracer_record
)
{
  gpu_activity_t gpu_activity;
  roctracer_activity_translate(&gpu_activity, roctracer_record);
  if (gpu_correlation_id_map_lookup(roctracer_record->correlation_id) == NULL) {
    gpu_correlation_id_map_insert(roctracer_record->correlation_id,
				  roctracer_record->correlation_id);
  }
  gpu_activity_process(&gpu_activity);
}


static void
roctracer_buffer_completion_callback
(
 const char* begin,
 const char* end,
 void* arg
)
{
  roctracer_buffer_completion_notify();
  roctracer_record_t* record = (roctracer_record_t*)(begin);
  roctracer_record_t* end_record = (roctracer_record_t*)(end);
  while (record < end_record) {
    roctracer_activity_process(record);
    record++;
  }
}


static const char *
roctracer_path
(
 void
)
{
  const char *path = "libroctracer64.so";

  return path;
}



//******************************************************************************
// interface operations
//******************************************************************************

int
roctracer_bind
(
 void
)
{
  // This is a workaround for roctracer to not hang when taking timer interrupts
  // More details: https://github.com/ROCm-Developer-Tools/roctracer/issues/22
  setenv("HSA_ENABLE_INTERRUPT", "0", 1);
  
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(roctracer, roctracer_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCTRACER_BIND(fn) \
  CHK_DLSYM(roctracer, fn);

  FORALL_ROCTRACER_ROUTINES(ROCTRACER_BIND)

#undef ROCTRACER_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}

void
roctracer_init
(
 void
)
{
  roctracer_properties_t properties;
  properties.buffer_size = 0x1000;
  properties.buffer_callback_fun = roctracer_buffer_completion_callback;
  properties.mode = 0;
  properties.alloc_fun = 0;
  properties.alloc_arg = 0;
  properties.buffer_callback_arg = 0;
  HPCRUN_ROCTRACER_CALL(roctracer_open_pool_expl,(&properties, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_callback,
			(roctracer_subscriber_callback, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_activity_expl, (NULL));
}

void
roctracer_fini
(
 void* args
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_disable_callback, ());
  HPCRUN_ROCTRACER_CALL(roctracer_disable_activity, ());
  HPCRUN_ROCTRACER_CALL(roctracer_flush_activity_expl, (NULL));

  gpu_application_thread_process_activities();
}

