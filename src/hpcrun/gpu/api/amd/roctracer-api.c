// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "roctracer-api.h"
#include "roctracer-activity-translate.h"

#include "hip-api.h"
#include "../../../foil/roctracer.h"
#include "../../../foil/rocm-hip.h"
#include "../../../foil/rocm-hsa.h"

#include "../../activity/gpu-activity-channel.h"
#include "../../activity/gpu-activity-process.h"
#include "../../activity/gpu-op-ccts-map.h"
#include "../../activity/correlation/gpu-correlation-channel.h"
#include "../../activity/correlation/gpu-host-correlation-map.h"
#include "../../gpu-monitoring-thread-api.h"
#include "../../gpu-application-thread-api.h"
#include "../../trace/gpu-trace-api.h"
#include "../../activity/gpu-op-placeholders.h"
#include "../common/gpu-cct.h"
#include "../common/gpu-kernel-table.h"


#include "../../../safe-sampling.h"

#include "../../../utilities/hpcrun-nanotime.h"
#include "../../../libmonitor/monitor.h"




#include "rocprofiler-api.h"

#include <hsa/hsa.h>

//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0
#include "../../common/gpu-print.h"

#define HPCRUN_ROCTRACER_CALL(fn, args) \
{      \
  roctracer_status_t status = f_##fn args;  \
  if (status != ROCTRACER_STATUS_SUCCESS) {    \
    /* use roctracer_error_string() */ \
  }            \
}



//******************************************************************************
// local variables
//******************************************************************************

// If we collect counters for GPU kernels,
// we will serialize kernel executions.
// Hopefully, AMD tool support will improve this the future
static bool collect_counter = false;


//******************************************************************************
// private operations
//******************************************************************************

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
  bool is_kernel_op = false;
  const hip_api_data_t* data = (const hip_api_data_t*)(callback_data);
  hipStream_t kernel_stream = 0;

  switch(domain) {
  case ACTIVITY_DOMAIN_HIP_API:
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
    case HIP_API_ID_hipHccModuleLaunchKernel: {
      //case HIP_API_ID_hipExtModuleLaunchKernel:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                   gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                   gpu_placeholder_type_trace);
      is_valid_op = true;
      is_kernel_op = true;
      if (collect_counter) {
        kernel_stream = data->args.hipModuleLaunchKernel.stream;
      }
      break;
    }
    case HIP_API_ID_hipLaunchKernel: {
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                   gpu_placeholder_type_kernel);
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                   gpu_placeholder_type_trace);
      is_valid_op = true;
      is_kernel_op = true;
      if (collect_counter) {
        kernel_stream = data->args.hipLaunchKernel.stream;
      }
      break;
    }
    case HIP_API_ID_hipCtxSynchronize:
    case HIP_API_ID_hipStreamSynchronize:
    case HIP_API_ID_hipDeviceSynchronize:
    case HIP_API_ID_hipEventSynchronize:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
                                   gpu_placeholder_type_sync);
      is_valid_op = true;
      break;
    default:
      PRINT("HIP API tracing: Unhandled op %u, domain %u\n", callback_id, domain);
      break;
    }
  case ACTIVITY_DOMAIN_HSA_API:
  case ACTIVITY_DOMAIN_KFD_API:
  case ACTIVITY_DOMAIN_ROCTX:
  default:
    break;
  }

  if (!is_valid_op) return;


  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    uint64_t correlation_id = data->correlation_id;
    uint64_t rocprofiler_correlation_id = 0;
    cct_node_t *api_node =
      gpu_application_thread_correlation_callback_rt1(correlation_id);

    gpu_op_ccts_t gpu_op_ccts;
    hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);

    if (is_kernel_op && collect_counter) {
      rocprofiler_correlation_id = correlation_id;
      rocprofiler_start_kernel(rocprofiler_correlation_id);
    }

    hpcrun_safe_exit();

    gpu_application_thread_process_activities();

    // Generate notification entry
    uint64_t cpu_submit_time = hpcrun_nanotime();
    uint64_t hsa_timestamp;
    f_hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP, &hsa_timestamp);

    // We align CPU & GPU timestamps
    gpu_set_cpu_gpu_timestamp(cpu_submit_time, hsa_timestamp);

    //gpu_monitors_apply(api_node, gpu_monitor_type_enter);

    gpu_op_ccts_map_insert(correlation_id, (gpu_op_ccts_map_entry_value_t) {
      .gpu_op_ccts = gpu_op_ccts,
      .cpu_submit_time = cpu_submit_time
    });

    gpu_correlation_channel_send(
      ROCTRACER_CHANNEL_IDX,
      correlation_id,
      gpu_activity_channel_get_local());

    if (collect_counter && is_kernel_op) {
      gpu_correlation_channel_send(
        ROCPROFILER_CHANNEL_IDX,
        rocprofiler_correlation_id,
        gpu_activity_channel_get_local());
    }

  } else if (data->phase == ACTIVITY_API_PHASE_EXIT) {
    if (is_kernel_op && collect_counter) {
      //gpu_monitors_apply(NULL, gpu_monitor_type_exit);
      f_hipStreamSynchronize(kernel_stream);
      rocprofiler_wait_context_callback();
      rocprofiler_stop_kernel();
    }
  }
}


static void
roctracer_buffer_completion_notify
(
  void
)
{
  gpu_monitoring_thread_activities_ready_with_idx(ROCTRACER_CHANNEL_IDX);
}


static void
roctracer_activity_process
(
 roctracer_record_t* roctracer_record
)
{
  // As of April 4th, 2022, enabling rocprofiler
  // causes roctracer to generate a dubious GPU kernel record.
  // The dubious GPU kernel record has a very small start timestamp,
  // causing a huge duration.
  // We drop roctracer records when the user collects
  // AMD GPU hardware counters.
  //
  // In rocm-5.1, rocprof does not collect HIP GPU traces and
  // hardware counters at the same time. It is unclear whether
  // this is a fundamental limitation of rocm or not.
  //
  // Therefore, at this point, it is safer to simply drop roctracer records
  // when using rocprofiler
  if (collect_counter) return;

  gpu_activity_t gpu_activity;
  uint64_t correlation_id;
  roctracer_activity_translate(&gpu_activity, roctracer_record, &correlation_id);

  if (correlation_id == 0) {
    return;
  }

  gpu_host_correlation_map_entry_t *host_op_entry =
    gpu_host_correlation_map_lookup(correlation_id);
  if (host_op_entry == NULL) {
    return;
  }

  gpu_activity_channel_t *channel =
    gpu_host_correlation_map_entry_channel_get(host_op_entry);
  if (channel == NULL) {
    return;
  }

  gpu_activity_channel_send(channel, &gpu_activity);
}


static void
roctracer_buffer_completion_callback
(
 const char* begin,
 const char* end,
 void* arg
)
{
  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);
  roctracer_buffer_completion_notify();
  roctracer_record_t* record = (roctracer_record_t*)(begin);
  roctracer_record_t* end_record = (roctracer_record_t*)(end);
  while (record < end_record) {
    roctracer_activity_process(record);
    record++;
  }
}

//******************************************************************************
// interface operations
//******************************************************************************

void
roctracer_init
(
 void
)
{
  gpu_kernel_table_init();

  monitor_disable_new_threads();

  HPCRUN_ROCTRACER_CALL(roctracer_set_properties, (ACTIVITY_DOMAIN_HIP_API, NULL));

  // Allocating tracing pool
  roctracer_properties_t properties;
  memset(&properties, 0, sizeof(roctracer_properties_t));
  properties.buffer_size = 0x1000;
  properties.buffer_callback_fun = roctracer_buffer_completion_callback;
  HPCRUN_ROCTRACER_CALL(roctracer_open_pool_expl, (&properties, NULL));

  // Enable HIP API callbacks
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_HIP_API, roctracer_subscriber_callback, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_HSA_API, roctracer_subscriber_callback, NULL));

  // Enable HIP activity tracing
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_activity_expl, (ACTIVITY_DOMAIN_HIP_API, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_activity_expl, (ACTIVITY_DOMAIN_HIP_OPS, NULL));

  // Enable PC sampling
  // HPCRUN_ROCTRACER_CALL(roctracer_enable_op_activity, (ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_PCSAMPLE));

  // Enable KFD API tracing
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_KFD_API, roctracer_subscriber_callback, NULL));

  // Enable rocTX
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_ROCTX, roctracer_subscriber_callback, NULL));

  // Prepare getting URI
  rocprofiler_uri_setup();

  monitor_enable_new_threads();
}

void
roctracer_flush
(
 void* args,
 int how
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_flush_activity_expl, (NULL));

  gpu_application_thread_process_activities();
}


void
roctracer_fini
(
 void* args,
 int how
)
{
  // Disable the roctracer infrastructure, in reverse order from the enables
  // Do we tear down the URI?

  // Disable rocTX
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_ROCTX));

  // Disable KFD API tracing
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_KFD_API));

  // Disable PC sampling
  // HPCRUN_ROCTRACER_CALL(roctracer_disable_op_activity, (ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_PCSAMPLE));

  // Disable HIP activity tracing
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_activity, (ACTIVITY_DOMAIN_HIP_OPS));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_activity, (ACTIVITY_DOMAIN_HIP_API));

  // Disable HIP API callbacks
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_HSA_API));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_HIP_API));

  // Do we deallocate the tracing pool?

  roctracer_flush(args, how);
}

void
roctracer_enable_counter_collection
(
  void
)
{
  collect_counter = true;
}
