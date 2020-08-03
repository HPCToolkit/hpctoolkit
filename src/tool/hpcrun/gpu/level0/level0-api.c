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
#include "level0-data-node.h"

#include <hpcrun/main.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

//******************************************************************************
// macros
//******************************************************************************
#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

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

#define HPCRUN_LEVEL0_CALL(fn, args) (LEVEL0_FN_NAME(fn) args)

//******************************************************************************
// local variables
//******************************************************************************

// Assume one driver, one device, and one tracer.
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

static void
get_gpu_driver_and_device
(
  void
)
{
  uint32_t driverCount = 0;
  uint32_t i, d;
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, NULL));

  ze_driver_handle_t* allDrivers = (ze_driver_handle_t*)hpcrun_malloc_safe(driverCount * sizeof(ze_driver_handle_t));
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, allDrivers));

  // Find a driver instance with a GPU device
  for(i = 0; i < driverCount; ++i) {
      uint32_t deviceCount = 0;
      HPCRUN_LEVEL0_CALL(zeDeviceGet, (allDrivers[i], &deviceCount, NULL));

      ze_device_handle_t* allDevices = (ze_device_handle_t*)hpcrun_malloc_safe(deviceCount * sizeof(ze_device_handle_t));
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
      if(NULL != hDriver) {
          break;
      }
  }

  if(NULL == hDevice) {
      fprintf(stderr, "NO GPU Device found\n");
      exit(0);
  }
}

static void
level0_event_pool_create_entry
(
  ze_event_pool_create_params_t *params,
  ze_result_t result,
  void *global_data,
  void **instance_data
)
{
  const ze_event_pool_desc_t* desc = *(params->pdesc);
  if (desc == NULL) {
    // Based on Level 0 header file,
    // zeEventPoolCreate will return ZE_RESULT_ERROR_INVALID_NULL_POINTER for this caes.
    // Therefore, we do nothing in this case.
    return;
  }

  // Here we need to allocate a new event pool descriptor
  // as we cannot directly change the passed in object (declared ad const)
  // This leads to one description per event pool creation.
  ze_event_pool_desc_t* pool_desc = (ze_event_pool_desc_t*) hpcrun_malloc_safe(sizeof(ze_event_pool_desc_t));
  pool_desc->version = desc->version;
  pool_desc->flags = desc->flags;
  pool_desc->count = desc->count;

  // We attach the time stamp flag to the event pool,
  // so that we can query time stamps for events in this pool.
  int flags = pool_desc->flags | ZE_EVENT_POOL_FLAG_TIMESTAMP;
  pool_desc->flags = (ze_event_pool_flag_t)(flags);
  *(params->pdesc) = pool_desc;

}

static void
attribute_event
(
  ze_event_handle_t event
)
{
  level0_data_node_t* data = level0_event_map_lookup(event);
  if (data == NULL) return;

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

  // Attribute this event
  level0_command_end(data, start, end);

  // We need to release the event and event_pool to level 0
  // if they are created by us.
  if (data->event_pool != NULL) {
    HPCRUN_LEVEL0_CALL(zeEventDestroy, (event));
    HPCRUN_LEVEL0_CALL(zeEventPoolDestroy,(data->event_pool));
  }

  // Free data structure for this event
  level0_event_map_delete(event);
}

static void
level0_event_destroy_entry
(
  ze_event_destroy_params_t *params,
  ze_result_t result,
  void *global_data,
  void **instance_data
)
{
  attribute_event(*(params->phEvent));
}

static void
level0_event_host_reset_entry
(
  ze_event_host_reset_params_t *params,
  ze_result_t result,
  void *global_data,
  void **instance_data
)
{
  attribute_event(*(params->phEvent));
}

static void
create_new_event
(
  ze_event_handle_t* event_ptr,
  ze_event_pool_handle_t* event_pool_ptr
)
{
  ze_event_pool_desc_t event_pool_desc = {
    ZE_EVENT_POOL_DESC_VERSION_CURRENT,
    ZE_EVENT_POOL_FLAG_TIMESTAMP,
    1 };
  HPCRUN_LEVEL0_CALL(zeEventPoolCreate, (hDriver, &event_pool_desc, 1, &hDevice, event_pool_ptr));

  ze_event_desc_t event_desc = {
    ZE_EVENT_DESC_VERSION_CURRENT,
    0,
    ZE_EVENT_SCOPE_FLAG_HOST,
    ZE_EVENT_SCOPE_FLAG_HOST };
  HPCRUN_LEVEL0_CALL(zeEventCreate, (*event_pool_ptr, &event_desc, event_ptr));
}

static void
level0_command_list_append_launch_kernel_entry
(
  ze_command_list_append_launch_kernel_params_t* params,
  ze_result_t result,
  void* global_data,
  void** instance_data
)
{
  ze_kernel_handle_t kernel = *(params->phKernel);
  ze_command_list_handle_t command_list = *(params->phCommandList);
  ze_event_handle_t event = *(params->phSignalEvent);
  ze_event_pool_handle_t event_pool = NULL;

  if (event == NULL) {
    // If the kernel is launched without an event,
    // we create a new event for collecting time stamps
    create_new_event(&event, &event_pool);
    *(params->phSignalEvent) = event;
  }

  PRINT("level0_command_list_append_launch_kernel_entry: kernel handle %p, commmand list handle %p, event handle %p, event pool handle %p\n",
    (void*)kernel, (void*)command_list, (void*)event, (void*)event_pool);

  // Lookup the command list and append the kernel launch to the command list
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  if (command_list_data_head != NULL) {
    level0_data_node_t * data_for_kernel = level0_commandlist_append_kernel(command_list_data_head, kernel, event, event_pool);
    // Associate the data entry with the event
    level0_event_map_insert(event, data_for_kernel);
  } else {
    // Cannot find command list.
    // This means we are dealing with an immediate command list
    level0_data_node_t * data_for_kernel = level0_commandlist_alloc_kernel(kernel, event, event_pool);;
    // Associate the data entry with the event
    level0_event_map_insert(event, data_for_kernel);
    // For immediate command list, the kernel is dispatched to GPU at this point.
    // So, we attribute GPU metrics to the current CPU calling context.
    level0_command_begin(data_for_kernel);
  }
}

static void
level0_command_list_append_launch_memcpy_entry
(
  ze_command_list_append_memory_copy_params_t* params,
  ze_result_t result,
  void* global_data,
  void** instance_data
)
{
  ze_command_list_handle_t command_list = *(params->phCommandList);
  ze_event_handle_t event = *(params->phEvent);
  ze_event_pool_handle_t event_pool = NULL;
  size_t mem_copy_size = *(params->psize);
  const void* dest_ptr = *(params->pdstptr);
  const void* src_ptr = *(params->psrcptr);

  if (event == NULL) {
    // If the memcpy is launched without an event,
    // we create a new event for collecting time stamps
    create_new_event(&event, &event_pool);
    *(params->phEvent) = event;
  }

  // Get source and destination type.
  // Level 0 does not track memory allocated through system allocator such as malloc.
  // In such case, zeDriverGetMemAllocProperties will return failure.
  // So, we default the memory type to be HOST.
  ze_memory_type_t src_type = ZE_MEMORY_TYPE_HOST;
  ze_memory_allocation_properties_t property;
  if (HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, src_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    src_type = property.type;
  }
  ze_memory_type_t dst_type = ZE_MEMORY_TYPE_HOST;
  if (HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, dest_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    dst_type = property.type;
  }

  PRINT("level0_command_list_append_launch_memcpy_entry: src_type %d, dst_type %d, size %lu, command list %p, event handle %p, event pool handle %p\n",
    src_type, dst_type, mem_copy_size, (void*)command_list, (void*)event, (void*)event_pool);

  // Lookup the command list and append the mempcy to the command list
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  if (command_list_data_head != NULL) {
    level0_data_node_t * data_for_memcpy = level0_commandlist_append_memcpy(command_list_data_head, src_type, dst_type, mem_copy_size, event, event_pool);
    // Associate the data entry with the event
    level0_event_map_insert(event, data_for_memcpy);
  } else {
    // Cannot find command list.
    // This means we are dealing with an immediate command list
    level0_data_node_t * data_for_memcpy = level0_commandlist_alloc_memcpy(src_type, dst_type, mem_copy_size, event, event_pool);
    // Associate the data entry with the event
    level0_event_map_insert(event, data_for_memcpy);
    // For immediate command list, the mempcy is dispatched to GPU at this point.
    // So, we attribute GPU metrics to the current CPU calling context.
    level0_command_begin(data_for_memcpy);
  }
}

static void
level0_command_list_append_barrier_entry
(
  ze_command_list_append_barrier_params_t* params,
  ze_result_t result,
  void* global_data,
  void** instance_data
)
{
  PRINT("Enter unimplemented level0_command_list_append_barrier_entry\n");
}

static void
level0_command_list_create_exit
(
  ze_command_list_create_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  ze_command_list_handle_t handle = **params->pphCommandList;
  PRINT("level0_command_list_create_exit: command list %p\n", (void*)handle);
  // Record the creation of a command list
  level0_commandlist_map_insert(handle);
}

static void
level0_command_list_destroy_entry
(
  ze_command_list_destroy_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  ze_command_list_handle_t handle = *params->phCommandList;
  level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(handle);
  level0_data_node_t * command_node = *command_list_head;
  for (; command_node != NULL; command_node = command_node->next) {
    attribute_event(command_node->event);
  }

  // Record the deletion of a command list
  level0_commandlist_map_delete(handle);
}

static void
level0_command_queue_execute_command_list_entry
(
  ze_command_queue_execute_command_lists_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  // We associate GPU metrics for GPU activitities in non-immediate command list
  // to the CPU call contexts where the command list is executed, not where
  // the GPU activity is appended.
  uint32_t size = *(params->pnumCommandLists);
  uint32_t i;
  for (i = 0; i < size; ++i) {
    ze_command_list_handle_t command_list_handle = *(params->pphCommandLists[i]);
    PRINT("level0_command_queue_execute_command_list_entry: command list %p\n", (void*)command_list_handle);
    level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(command_list_handle);
    level0_data_node_t * command_node = *command_list_head;
    for (; command_node != NULL; command_node = command_node->next) {
      level0_command_begin(command_node);
    }
  }
}

static void
process_immediate_command_list
(
  ze_event_handle_t event,
  ze_command_list_handle_t command_list
)
{
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  if (command_list_data_head == NULL) {
    // This is a GPU activity to an immediate command list
    level0_data_node_t* data_for_act = level0_event_map_lookup(event);
    attribute_event(event);

    // For command in immediate command list,
    // the ownership of data node belongs to the user, not the command list
    level0_data_node_return_free_list(data_for_act);
  }
}

static void
level0_command_list_append_launch_kernel_exit(
  ze_command_list_append_launch_kernel_params_t* params,
  ze_result_t result,
  void* global_data,
  void** instance_data)
{
  process_immediate_command_list(*(params->phSignalEvent), *(params->phCommandList));
}

static void
level0_command_list_append_memcpy_exit
(
  ze_command_list_append_memory_copy_params_t* params,
  ze_result_t result,
  void* global_data,
  void** instance_data
)
{
  process_immediate_command_list(*(params->phEvent), *(params->phCommandList));
}

static void
level0_command_list_append_memfill_exit
(
  ze_command_list_append_memory_fill_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  PRINT("Enter unimplemented OnExitCommandListAPpendMemoryFill\n");
}

static void
level0_command_list_append_memcpy_region_exit
(
  ze_command_list_append_memory_copy_region_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  PRINT("Enter unimplemented level0_command_list_append_memcpy_region_exit\n");
}

static void
OnEnterEventHostSignal
(
  ze_event_host_signal_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  PRINT("Enter OnEnterEvenHostSignal on event %p\n");
}

static void
OnEnterEventHostSynchronize
(
  ze_event_host_synchronize_params_t* params,
  ze_result_t result,
  void* pTracerUserData,
  void** ppTracerInstanceUserData
)
{
  PRINT("Enter OnEnterEventHostSynchronize on event %p\n");
}

void setup_tracer() {
  zet_tracer_desc_t tracer_desc;
  tracer_desc.version = ZET_TRACER_DESC_VERSION_CURRENT;
  tracer_desc.pUserData = NULL;
  HPCRUN_LEVEL0_CALL(zetTracerCreate, (hDriver, &tracer_desc, &hTracer));

  // Set all callbacks
  zet_core_callbacks_t prologue_callbacks = {};
  prologue_callbacks.Event.pfnDestroyCb = level0_event_destroy_entry;
  prologue_callbacks.Event.pfnHostResetCb = level0_event_host_reset_entry;
  prologue_callbacks.Event.pfnHostSignalCb = OnEnterEventHostSignal;
  prologue_callbacks.Event.pfnHostSynchronizeCb = OnEnterEventHostSynchronize;
  prologue_callbacks.EventPool.pfnCreateCb = level0_event_pool_create_entry;

  prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    level0_command_list_append_launch_kernel_entry;
  prologue_callbacks.CommandList.pfnAppendMemoryCopyCb =
    level0_command_list_append_launch_memcpy_entry;
  prologue_callbacks.CommandList.pfnAppendBarrierCb =
    level0_command_list_append_barrier_entry;
  prologue_callbacks.CommandList.pfnDestroyCb = level0_command_list_destroy_entry;
  prologue_callbacks.CommandQueue.pfnExecuteCommandListsCb = level0_command_queue_execute_command_list_entry;

  zet_core_callbacks_t epilogue_callbacks = {};
  epilogue_callbacks.CommandList.pfnCreateCb = level0_command_list_create_exit;
  epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    level0_command_list_append_launch_kernel_exit;
  epilogue_callbacks.CommandList.pfnAppendMemoryCopyCb =
    level0_command_list_append_memcpy_exit;
  epilogue_callbacks.CommandList.pfnAppendMemoryFillCb = level0_command_list_append_memfill_exit;
  epilogue_callbacks.CommandList.pfnAppendMemoryCopyRegionCb = level0_command_list_append_memcpy_region_exit;

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
  // Enable level 0 API tracing
  setenv("ZE_ENABLE_API_TRACING", "1", 1);

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
  gpu_application_thread_process_activities();
  HPCRUN_LEVEL0_CALL(zetTracerSetEnabled, (hTracer, 0));
  HPCRUN_LEVEL0_CALL(zetTracerDestroy, (hTracer));
}

