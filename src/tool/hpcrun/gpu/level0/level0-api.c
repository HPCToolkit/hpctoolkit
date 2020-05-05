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

#include "level0-api.h"

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
#include <lib/prof-lean/usec_time.h>


#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>


//******************************************************************************
// macros
//******************************************************************************

#define FORALL_LEVEL0_ROUTINES(macro)			\
  macro(zeInit)   \
  macro(zetInit)  \
  macro(zeDriverGet)  \
  macro(zeDeviceGet) \
  macro(zeDeviceGetProperties)   \
  macro(zetTracerCreate) \
  macro(zetTracerSetPrologues) \
  macro(zetTracerSetEpilogues) \
  macro(zetTracerSetEnabled) \
  macro(zetTracerDestroy)


#define LEVEL0_FN_NAME(f) DYN_FN_NAME(f)

#define LEVEL0_FN(fn, args) \
  static ze_result_t (*LEVEL0_FN_NAME(fn)) args

#define HPCRUN_LEVEL0_CALL(fn, args) \
{      \
  ze_result_t status = LEVEL0_FN_NAME(fn) args;	\
  if (status != ZE_RESULT_SUCCESS) {		\
    /* use level0_error_string() */ \
  }						\
}

#define CPU_NANOTIME() (usec_time() * 1000)



//******************************************************************************
// local variables
//******************************************************************************

ze_driver_handle_t hDriver = NULL;
ze_device_handle_t hDevice = NULL;
zet_tracer_handle_t hTracer = NULL;
static uint64_t correlation_id = 0;

//----------------------------------------------------------
// level0 function pointers for late binding
//----------------------------------------------------------

LEVEL0_FN
(
 zeInit,
 (
  ze_init_flag_t  
 )
);

LEVEL0_FN
(
 zetInit,
 (
  ze_init_flag_t
 )
);

LEVEL0_FN
(
 zeDriverGet,
 (
  uint32_t*,
  ze_driver_handle_t*
 )
);

LEVEL0_FN
(
 zeDeviceGet,
 (
  ze_driver_handle_t,
  uint32_t*,
  ze_device_handle_t*
 )
);

LEVEL0_FN
(
 zeDeviceGetProperties,
 (
  ze_device_handle_t,
  ze_device_properties_t*
 )
);

LEVEL0_FN
(
 zetTracerCreate,
 (
  zet_driver_handle_t,
  const zet_tracer_desc_t*,
  zet_tracer_handle_t*
 )
);

LEVEL0_FN
(
 zetTracerSetPrologues,
 (
  zet_tracer_handle_t,
  zet_core_callbacks_t*
 )
);

LEVEL0_FN
(
 zetTracerSetEpilogues,
 (
  zet_tracer_handle_t,
  zet_core_callbacks_t*
 )
);

LEVEL0_FN
(
 zetTracerSetEnabled,
 (
  zet_tracer_handle_t,
  ze_bool_t
 )
);

LEVEL0_FN
(
 zetTracerDestroy,
 (
  zet_tracer_handle_t  
 )
);


//******************************************************************************
// private operations
//******************************************************************************

static const char *
level0_path
(
 void
)
{
  const char *path = "libze_loader.so";

  return path;
}

void
get_gpu_driver_and_device
(
  void
)
{
  uint32_t driverCount = 0;
  uint32_t i, d;
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, NULL));
  
  ze_driver_handle_t* allDrivers = (ze_driver_handle_t*)malloc(driverCount * sizeof(ze_driver_handle_t));
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, allDrivers));
  
  // Find a driver instance with a GPU device
  for(i = 0; i < driverCount; ++i) {
      uint32_t deviceCount = 0;
      HPCRUN_LEVEL0_CALL(zeDeviceGet, (allDrivers[i], &deviceCount, NULL));
      
      ze_device_handle_t* allDevices = (ze_device_handle_t*)malloc(deviceCount * sizeof(ze_device_handle_t));
      HPCRUN_LEVEL0_CALL(zeDeviceGet, (allDrivers[i], &deviceCount, allDevices));
      
      for(d = 0; d < deviceCount; ++d) {
          ze_device_properties_t device_properties;
          HPCRUN_LEVEL0_CALL(zeDeviceGetProperties, (allDevices[d], &device_properties));  
          if(ZE_DEVICE_TYPE_GPU == device_properties.type) {
              hDriver = allDrivers[i];
              hDevice = allDevices[d];
              break;
          }
      }
      free(allDevices);
      if(NULL != hDriver) {
          break;
      }
  }
  
  free(allDrivers);
  if(NULL == hDevice) {
      fprintf(stderr, "NO GPU Device found\n");
      exit(0);
  }
}

typedef struct _my_tracer_data_t
{
    uint32_t instance;
} my_tracer_data_t;

typedef struct _my_instance_data_t
{
    struct timespec start;
} my_instance_data_t;

void OnEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result,
    void* pTracerUserData,
    void** ppTracerInstanceUserData )
{
    my_instance_data_t* instance_data = (my_instance_data_t*) malloc( sizeof(my_instance_data_t) );
    *ppTracerInstanceUserData = instance_data;

    clock_gettime(CLOCK_REALTIME, &instance_data->start);

    gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_kernel);
    correlation_id++;
    cct_node_t *api_node =
      gpu_application_thread_correlation_callback(correlation_id);

    gpu_op_ccts_t gpu_op_ccts;
    hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
    hpcrun_safe_exit();

    gpu_activity_channel_consume(gpu_metrics_attribute);

    // Generate notification entry
    uint64_t cpu_submit_time = CPU_NANOTIME();
    gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
}

void OnExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result,
    void* pTracerUserData,
    void** ppTracerInstanceUserData )
{
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);

    my_tracer_data_t* tracer_data = (my_tracer_data_t*)pTracerUserData;
    my_instance_data_t* instance_data = *(my_instance_data_t**)ppTracerInstanceUserData;

    gpu_monitoring_thread_activities_ready();
    gpu_activity_t gpu_activity;
    gpu_activity_t* ga = &gpu_activity;
    memset(ga, 0, sizeof(gpu_activity_t));

    ga->kind = GPU_ACTIVITY_KERNEL;

    #define CONVERT(x) ((uint64_t)((x).tv_sec) * (uint64_t)1000000000 + (uint64_t)((x).tv_nsec))
    set_gpu_interval(&ga->details.interval, CONVERT(instance_data->start) , CONVERT(end));
    ga->details.kernel.correlation_id = correlation_id;
    cstack_ptr_set(&(ga->next), 0);
    if (gpu_correlation_id_map_lookup(correlation_id) == NULL) {
      gpu_correlation_id_map_insert(correlation_id, correlation_id);
    }
    gpu_activity_process(&gpu_activity);
    free(instance_data);
}


void setup_tracer() {
  my_tracer_data_t tracer_data = {};
  zet_tracer_desc_t tracer_desc;
  tracer_desc.version = ZET_TRACER_DESC_VERSION_CURRENT;
  tracer_desc.pUserData = &tracer_data;
  HPCRUN_LEVEL0_CALL(zetTracerCreate, (hDriver, &tracer_desc, &hTracer));

  // Set all callbacks
  zet_core_callbacks_t prologCbs = {};
  zet_core_callbacks_t epilogCbs = {};
  prologCbs.CommandList.pfnAppendLaunchKernelCb = OnEnterCommandListAppendLaunchKernel;
  epilogCbs.CommandList.pfnAppendLaunchKernelCb = OnExitCommandListAppendLaunchKernel;
  HPCRUN_LEVEL0_CALL(zetTracerSetPrologues, (hTracer, &prologCbs));
  HPCRUN_LEVEL0_CALL(zetTracerSetEpilogues, (hTracer, &epilogCbs));

  // Enable tracing
  HPCRUN_LEVEL0_CALL(zetTracerSetEnabled, (hTracer, 1));
}


//******************************************************************************
// interface operations
//******************************************************************************

int
level0_bind
(
 void
)
{
  
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(level0, level0_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define LEVEL0_BIND(fn) \
  CHK_DLSYM(level0, fn);

  FORALL_LEVEL0_ROUTINES(LEVEL0_BIND)

#undef LEVEL0_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}

void
level0_init
(
 void
)
{  
  HPCRUN_LEVEL0_CALL(zeInit,(ZE_INIT_FLAG_NONE));
  HPCRUN_LEVEL0_CALL(zetInit, (ZE_INIT_FLAG_NONE));
  get_gpu_driver_and_device();
  setup_tracer();
}

void
level0_fini
(
 void* args
)
{
  HPCRUN_LEVEL0_CALL(zetTracerSetEnabled, (hTracer, 0));
  HPCRUN_LEVEL0_CALL(zetTracerDestroy, (hTracer));

  gpu_application_thread_process_activities();
}

