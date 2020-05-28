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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "level0-api.h"
#include "level0-command-list-map.h"
#include "level0-event-map.h"
#include "level0-command-process.h"

#include <hpcrun/main.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>

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
  macro(zetTracerDestroy) \
  macro(zeEventCreate) \
  macro(zeEventDestroy) \
  macro(zeEventPoolCreate) \
  macro(zeEventPoolDestroy) \
  macro(zeEventQueryStatus) \
  macro(zeEventGetTimestamp) \
  macro(zeDriverGetMemAllocProperties)


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

//******************************************************************************
// local variables
//******************************************************************************

ze_driver_handle_t hDriver = NULL;
ze_device_handle_t hDevice = NULL;
zet_tracer_handle_t hTracer = NULL;

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

LEVEL0_FN
(
 zeEventCreate,
 (
  ze_event_pool_handle_t,
  const ze_event_desc_t*,
  ze_event_handle_t*
 )
);

LEVEL0_FN
(
 zeEventDestroy,
 (
   ze_event_handle_t
 )
);

LEVEL0_FN
(
 zeEventPoolCreate,
 (
  ze_driver_handle_t,
  const ze_event_pool_desc_t*,
  uint32_t,
  ze_device_handle_t*,
  ze_event_pool_handle_t*
 )
);

LEVEL0_FN
(
 zeEventPoolDestroy,
 (
   ze_event_pool_handle_t
 )
);

LEVEL0_FN
(
 zeEventQueryStatus,
 (
   ze_event_handle_t
 )
);

LEVEL0_FN
(
 zeEventGetTimestamp,
 (
   ze_event_handle_t,
   ze_event_timestamp_type_t,
   void*
 )
);

LEVEL0_FN
(
 zeDriverGetMemAllocProperties,
 (
    ze_driver_handle_t,
    const void* ptr,
    ze_memory_allocation_properties_t*,
    ze_device_handle_t*
  );
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

typedef enum EventType {
  EVENT_TYPE_USER = 0,
  EVENT_TYPE_TOOL = 1
} EventType;

typedef struct ActivityEventInfo {
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  EventType event_type;
} ActivityEventInfo;

static void OnEnterEventPoolCreate(ze_event_pool_create_params_t *params,
                                   ze_result_t result,
                                   void *global_data,
                                   void **instance_data) {
  const ze_event_pool_desc_t* desc = *(params->pdesc);
  if (desc == NULL) {
    return;
  }

  ze_event_pool_desc_t* profiling_desc = (ze_event_pool_desc_t*) malloc(sizeof(ze_event_pool_desc_t));
  assert(profiling_desc != NULL);
  profiling_desc->version = desc->version;
  profiling_desc->flags = desc->flags;
  profiling_desc->count = desc->count;

  int flags = profiling_desc->flags | ZE_EVENT_POOL_FLAG_TIMESTAMP;
  profiling_desc->flags = (ze_event_pool_flag_t)(flags);

  *(params->pdesc) = profiling_desc;
  *instance_data = profiling_desc;
}

static void OnExitEventPoolCreate(ze_event_pool_create_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
  ze_event_pool_desc_t* desc = (ze_event_pool_desc_t*)(*instance_data);
  if (desc != NULL) {
    free(desc);
  }
}

static void ProcessEvent(ze_event_handle_t event) {
  level0_data_node_t* data = level0_event_map_lookup(event);  
  if (data == NULL) return;
  fprintf(stderr, "ProcessEvent %p %p\n", (void*) event, (void*)(data->event) );  

  // Get ready to query time stamps
  ze_device_properties_t props = {};
  props.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
  HPCRUN_LEVEL0_CALL(zeDeviceGetProperties, (hDevice, &props));
  HPCRUN_LEVEL0_CALL(zeEventQueryStatus, (event));

  // Query start and end time stamp for the event
  uint64_t start = 0;
  HPCRUN_LEVEL0_CALL(zeEventGetTimestamp, (event, ZE_EVENT_TIMESTAMP_CONTEXT_START, &start));
  start = start * props.timerResolution;
  uint64_t end = 0;
  HPCRUN_LEVEL0_CALL(zeEventGetTimestamp, (event, ZE_EVENT_TIMESTAMP_CONTEXT_END, &end));
  end = end * props.timerResolution;

  level0_command_end(data, start, end);

  level0_event_map_delete(event);

  //if (info.event_type == EVENT_TYPE_TOOL) {
    //HPCRUN_LEVEL0_CALL(zeEventDestroy, (event));
    //HPCRUN_LEVEL0_CALL(zeEventPoolDestroy,(info.event_pool));
  //}


}

static void OnEnterEventDestroy(ze_event_destroy_params_t *params,
                                ze_result_t result,
                                void *global_data,
                                void **instance_data) {
  ProcessEvent(*(params->phEvent));
}

static void OnEnterEventHostReset(ze_event_host_reset_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
  ProcessEvent(*(params->phEvent));
}

static ze_event_handle_t OnEnterActivitySubmit(ze_event_handle_t event, void **instance_data) {
  ActivityEventInfo* info =  (ActivityEventInfo*) malloc(sizeof(ActivityEventInfo));

  if (event == NULL) {
    ze_event_pool_desc_t event_pool_desc = {
      ZE_EVENT_POOL_DESC_VERSION_CURRENT,
      ZE_EVENT_POOL_FLAG_TIMESTAMP,
      1 };
    ze_event_pool_handle_t event_pool = NULL;
    HPCRUN_LEVEL0_CALL(zeEventPoolCreate, (hDriver, &event_pool_desc, 1, &hDevice, &event_pool));

    ze_event_desc_t event_desc = {
      ZE_EVENT_DESC_VERSION_CURRENT,
      0,
      ZE_EVENT_SCOPE_FLAG_HOST,
      ZE_EVENT_SCOPE_FLAG_HOST };
    HPCRUN_LEVEL0_CALL(zeEventCreate, (event_pool, &event_desc, &event));

    info->event_pool = event_pool;
    info->event_type = EVENT_TYPE_TOOL;
  } else {
    info->event_pool = NULL;
    info->event_type = EVENT_TYPE_USER;
  }

  info->event = event;
  *instance_data = info;
  return event;
}

static void OnEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data)
{
  *(params->phSignalEvent) = OnEnterActivitySubmit(*(params->phSignalEvent), instance_data);

  ze_kernel_handle_t kernel = *(params->phKernel);
  ze_command_list_handle_t command_list = *(params->phCommandList);
  ze_event_handle_t event = *(params->phSignalEvent);

  // Lookup the command list and append the kernel-event pair to the command list
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  level0_data_node_t * data_for_kernel = level0_commandlist_append_kernel(command_list_data_head, kernel, event);
  // Associate the data entry with the event
  level0_event_map_insert(event, data_for_kernel);
}

static void OnEnterCommandListAppendMemoryCopy(
    ze_command_list_append_memory_copy_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  *(params->phEvent) = OnEnterActivitySubmit(*(params->phEvent), instance_data);
  
  ze_command_list_handle_t command_list = *(params->phCommandList);
  ze_event_handle_t event = *(params->phEvent);
  fprintf(stderr, "OnEnterCommandListAppendMemoryCopy command_list %p event %p\n", (void*)command_list, (void*) event);  
  size_t mem_copy_size = *(params->psize);
  const void* dest_ptr = *(params->pdstptr);
  const void* src_ptr = *(params->psrcptr);
  ze_memory_allocation_properties_t dest_property;
  ze_memory_allocation_properties_t src_property;
  HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, dest_ptr, &dest_property, NULL));
  HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, src_ptr, &src_property, NULL));

  // Lookup the command list and append the mempcy to the command list
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  level0_data_node_t * data_for_memcpy = level0_commandlist_append_memcpy(command_list_data_head, src_property.type, dest_property.type, mem_copy_size, event);
  // Associate the data entry with the event
  level0_event_map_insert(event, data_for_memcpy);
}

static void OnEnterCommandListAppendBarrier(
    ze_command_list_append_barrier_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
/*
  *(params->phSignalEvent) = OnEnterActivitySubmit(
      "<Barrier>", *(params->phSignalEvent), instance_data);
*/
}

static void OnExitActivitySubmit(void **instance_data, ze_result_t result) {
  
  ActivityEventInfo* info = (ActivityEventInfo*)(*instance_data);
  fprintf(stderr, "Enter ONExitActivitySummit %p, %d\n", info, info->event_type);
  if (info == NULL) {
    return;
  }

  if (result != ZE_RESULT_SUCCESS && info != NULL) {
    if (info->event_type == EVENT_TYPE_TOOL) {
      fprintf(stderr, "call zeEventDestroy in ONExitActivitySummit\n");
      HPCRUN_LEVEL0_CALL(zeEventDestroy, (info->event));
      HPCRUN_LEVEL0_CALL(zeEventPoolDestroy, (info->event_pool));
    }
  } else {
    fprintf(stderr, "In ONExitActivitySubmit not handled %p %d\n", info, info->event_type);
  }
  free(info);
}

static void
OnExitCommandListCreate
(
  ze_command_list_create_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData  
)
{  
  ze_command_list_handle_t handle = **params->pphCommandList;
  fprintf(stderr, "OnExitCommandListCreate command_list %p\n", (void*)handle);
  // Record the creation of a command list
  level0_commandlist_map_insert(handle);
}

static void
OnEnterCommandListDestroy
(
  ze_command_list_destroy_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  ze_command_list_handle_t handle = *params->phCommandList;
  fprintf(stderr, "OnEnterCommandListDestroy command_list %p\n", (void*)handle);
  level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(handle);
  level0_data_node_t * command_node = *command_list_head;
  for (; command_node != NULL; command_node = command_node->next) {
    ProcessEvent(command_node->event);
  }

  // Record the deletion of a command list
  level0_commandlist_map_delete(handle);  
}

static void
OnEnterCommandQueueExecuteCommandList
(
  ze_command_queue_execute_command_lists_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{ 
  uint32_t size = *(params->pnumCommandLists);
  uint32_t i;
  for (i = 0; i < size; ++i) {
    ze_command_list_handle_t command_list_handle = *(params->pphCommandLists[i]);
    level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(command_list_handle);
    level0_data_node_t * command_node = *command_list_head;
    for (; command_node != NULL; command_node = command_node->next) {      
      level0_command_begin(command_node);
    }
  }
}

static void OnExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  assert(*(params->phSignalEvent) != NULL);
  OnExitActivitySubmit(instance_data, result);
}

static void OnExitCommandListAppendMemoryCopy(
    ze_command_list_append_memory_copy_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  OnExitActivitySubmit(instance_data, result);
}

static void OnExitCommandListAppendBarrier(
    ze_command_list_append_barrier_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  //OnExitActivitySubmit(instance_data, result);
}

void setup_tracer() {
  zet_tracer_desc_t tracer_desc;
  tracer_desc.version = ZET_TRACER_DESC_VERSION_CURRENT;
  tracer_desc.pUserData = NULL;
  HPCRUN_LEVEL0_CALL(zetTracerCreate, (hDriver, &tracer_desc, &hTracer));

  // Set all callbacks
  zet_core_callbacks_t prologue_callbacks = {};
  prologue_callbacks.Event.pfnDestroyCb = OnEnterEventDestroy;
  prologue_callbacks.Event.pfnHostResetCb = OnEnterEventHostReset;
  prologue_callbacks.EventPool.pfnCreateCb = OnEnterEventPoolCreate;

  prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnEnterCommandListAppendLaunchKernel;
  prologue_callbacks.CommandList.pfnAppendMemoryCopyCb =
    OnEnterCommandListAppendMemoryCopy;
  prologue_callbacks.CommandList.pfnAppendBarrierCb =
    OnEnterCommandListAppendBarrier;
  prologue_callbacks.CommandList.pfnDestroyCb = OnEnterCommandListDestroy;
  prologue_callbacks.CommandQueue.pfnExecuteCommandListsCb = OnEnterCommandQueueExecuteCommandList;

  zet_core_callbacks_t epilogue_callbacks = {};
  //epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;
  //epilogue_callbacks.Kernel.pfnDestroyCb = OnExitKernelDestroy;
  epilogue_callbacks.EventPool.pfnCreateCb = OnExitEventPoolCreate;
  epilogue_callbacks.CommandList.pfnCreateCb = OnExitCommandListCreate;
  epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnExitCommandListAppendLaunchKernel;
  epilogue_callbacks.CommandList.pfnAppendMemoryCopyCb =
    OnExitCommandListAppendMemoryCopy;
  epilogue_callbacks.CommandList.pfnAppendBarrierCb =
    OnExitCommandListAppendBarrier;
  HPCRUN_LEVEL0_CALL(zetTracerSetPrologues, (hTracer, &prologue_callbacks));
  HPCRUN_LEVEL0_CALL(zetTracerSetEpilogues, (hTracer, &epilogue_callbacks));

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

