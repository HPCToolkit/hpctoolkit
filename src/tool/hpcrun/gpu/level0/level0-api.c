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
#include "level0-command-list-context-map.h"
#include "level0-event-map.h"
#include "level0-command-process.h"
#include "level0-data-node.h"

#include <hpcrun/main.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>

#include <level_zero/ze_api.h>

//******************************************************************************
// macros
//******************************************************************************
#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

#define FORALL_LEVEL0_ROUTINES(macro)			\
  macro(zeInit)   \
  macro(zeDriverGet)  \
  macro(zeDeviceGet) \
  macro(zeDeviceGetProperties)   \
  macro(zeEventCreate) \
  macro(zeEventDestroy) \
  macro(zeEventPoolCreate) \
  macro(zeEventPoolDestroy) \
  macro(zeEventQueryStatus) \
  macro(zeEventQueryKernelTimestamp) \
  macro(zeMemGetAllocProperties) \
  macro(zeCommandListAppendLaunchKernel) \
  macro(zeCommandListAppendMemoryCopy) \
  macro(zeCommandListCreate) \
  macro(zeCommandListCreateImmediate) \
  macro(zeCommandListDestroy) \
  macro(zeCommandQueueExecuteCommandLists) \
  macro(zeEventHostReset)

#define LEVEL0_FN_NAME(f) DYN_FN_NAME(f)

#define LEVEL0_FN(fn, args) \
  static ze_result_t (*LEVEL0_FN_NAME(fn)) args

#define HPCRUN_LEVEL0_CALL(fn, args) (LEVEL0_FN_NAME(fn) args)

//******************************************************************************
// local variables
//******************************************************************************

// Assume one driver and one device.
ze_driver_handle_t hDriver = NULL;
ze_device_handle_t hDevice = NULL;

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
  ze_context_handle_t,
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
 zeEventQueryKernelTimestamp,
 (
   ze_event_handle_t,
   ze_kernel_timestamp_result_t*
 )
);

LEVEL0_FN
(
 zeMemGetAllocProperties,
 (
    ze_context_handle_t,
    const void* ptr,
    ze_memory_allocation_properties_t*,
    ze_device_handle_t*
  )
);

LEVEL0_FN
(
  zeCommandListAppendLaunchKernel,
  (
    ze_command_list_handle_t hCommandList,          ///< [in] handle of the command list
    ze_kernel_handle_t hKernel,                     ///< [in] handle of the kernel object
    const ze_group_count_t* pLaunchFuncArgs,        ///< [in] thread group launch arguments
    ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                    ///< if `nullptr == phWaitEvents`
    ze_event_handle_t* phWaitEvents                 ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                    ///< on before launching
  )
);

LEVEL0_FN
(
  zeCommandListAppendMemoryCopy,
  (
    ze_command_list_handle_t hCommandList,          ///< [in] handle of command list
    void* dstptr,                                   ///< [in] pointer to destination memory to copy to
    const void* srcptr,                             ///< [in] pointer to source memory to copy from
    size_t size,                                    ///< [in] size in bytes to copy
    ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                    ///< if `nullptr == phWaitEvents`
    ze_event_handle_t* phWaitEvents                 ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                    ///< on before launching
  )
);

LEVEL0_FN
(
  zeCommandListCreate,
  (
    ze_context_handle_t hContext,                   ///< [in] handle of the context object
    ze_device_handle_t hDevice,                     ///< [in] handle of the device object
    const ze_command_list_desc_t* desc,             ///< [in] pointer to command list descriptor
    ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
  )
);

LEVEL0_FN
(
  zeCommandListCreateImmediate,
  (
    ze_context_handle_t hContext,                   ///< [in] handle of the context object
    ze_device_handle_t hDevice,                     ///< [in] handle of the device object
    const ze_command_queue_desc_t* desc,            ///< [in] pointer to command queue descriptor
    ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
  )
);

LEVEL0_FN
(
  zeCommandListDestroy,
  (
    ze_command_list_handle_t hCommandList           ///< [in][release] handle of command list object to destroy
  )
);

LEVEL0_FN
(
  zeCommandQueueExecuteCommandLists,
  (
    ze_command_queue_handle_t hCommandQueue,        ///< [in] handle of the command queue
    uint32_t numCommandLists,                       ///< [in] number of command lists to execute
    ze_command_list_handle_t* phCommandLists,       ///< [in][range(0, numCommandLists)] list of handles of the command lists
                                                    ///< to execute
    ze_fence_handle_t hFence                        ///< [in][optional] handle of the fence to signal on completion
  )
);

LEVEL0_FN
(
  zeEventHostReset,
  (
    ze_event_handle_t hEvent                        ///< [in] handle of the event
  )
);

//******************************************************************************
// private operations
//******************************************************************************

static void
level0_check_result
(
  ze_result_t ret,
  int lineNo
)
{
  if (ret == ZE_RESULT_SUCCESS) return;
  fprintf(stderr, "Level API at line %d failed:", lineNo - 1);
  #define LEVEL0_ERROR_CASE(x) case x: { fprintf(stderr, " %s", #x); break; }
  switch (ret) {
    LEVEL0_ERROR_CASE(ZE_RESULT_ERROR_UNINITIALIZED);
    LEVEL0_ERROR_CASE(ZE_RESULT_ERROR_DEVICE_LOST);
    LEVEL0_ERROR_CASE(ZE_RESULT_ERROR_INVALID_ENUMERATION);
    LEVEL0_ERROR_CASE(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
    default:
      fprintf(stderr, " unknown return code");
  }
  fprintf(stderr, "\n");
  exit(1);
}

static const char *
level0_path
(
  void
)
{
  static const char *path = "libze_loader.so";

  return path;
}

static void
get_gpu_driver_and_device
(
  void
)
{
  if (hDevice != NULL) return;
  uint32_t driverCount = 0;
  uint32_t i, d;
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, NULL));

  ze_driver_handle_t* allDrivers = (ze_driver_handle_t*)hpcrun_malloc_safe(driverCount * sizeof(ze_driver_handle_t));
  HPCRUN_LEVEL0_CALL(zeDriverGet, (&driverCount, allDrivers));
  PRINT("Get %d driver handles\n", driverCount);

  // Find a driver instance with a GPU device
  for(i = 0; i < driverCount; ++i) {
    uint32_t deviceCount = 0;
    HPCRUN_LEVEL0_CALL(zeDeviceGet, (allDrivers[i], &deviceCount, NULL));
    PRINT("\tGet %d device handles\n", deviceCount);

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
level0_create_new_event
(
  ze_context_handle_t hContext,
  ze_event_handle_t* event_ptr,
  ze_event_pool_handle_t* event_pool_ptr
)
{

  ze_event_pool_desc_t event_pool_desc = {
    ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
    NULL,
    ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, // all events in pool are kernel timestamps
    1 // count
  };
  HPCRUN_LEVEL0_CALL(zeEventPoolCreate, (hContext, &event_pool_desc, 1, &hDevice, event_pool_ptr));

  ze_event_desc_t event_desc = {
    ZE_STRUCTURE_TYPE_EVENT_DESC,
    NULL,
    0, // index
    0, // no memory/cache coherency required on signal
    0  // no memory/cache coherency required on wait
  };
  HPCRUN_LEVEL0_CALL(zeEventCreate, (*event_pool_ptr, &event_desc, event_ptr));
}

static void
level0_attribute_event
(
  ze_event_handle_t event
)
{
  level0_data_node_t* data = level0_event_map_lookup(event);
  if (data == NULL) return;

  // Get ready to query time stamps
  ze_device_properties_t props = {};
  HPCRUN_LEVEL0_CALL(zeDeviceGetProperties, (hDevice, &props));
  HPCRUN_LEVEL0_CALL(zeEventQueryStatus, (event));

  // Query start and end time stamp for the event
  ze_kernel_timestamp_result_t timestamp;
  HPCRUN_LEVEL0_CALL(zeEventQueryKernelTimestamp, (event, &timestamp));
  uint64_t start = timestamp.global.kernelStart * props.timerResolution;
  uint64_t end = timestamp.global.kernelEnd * props.timerResolution;

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
level0_get_memory_types
(
  ze_context_handle_t hContext,
  const void* src_ptr,
  const void* dest_ptr,
  ze_memory_type_t *src_type_ptr,
  ze_memory_type_t *dst_type_ptr
)
{
  // Get source and destination type.
  // Level 0 does not track memory allocated through system allocator such as malloc.
  // In such case, zeDriverGetMemAllocProperties will return failure.
  // So, we default the memory type to be HOST.
  ze_memory_allocation_properties_t property;
  if (HPCRUN_LEVEL0_CALL(zeMemGetAllocProperties, (hContext, src_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    *src_type_ptr = property.type;
  }
  if (HPCRUN_LEVEL0_CALL(zeMemGetAllocProperties, (hContext, dest_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    *dst_type_ptr = property.type;
  }
}
static void
level0_event_pool_create_entry
(
  const ze_event_pool_desc_t* desc,
  ze_event_pool_desc_t* pool_desc
)
{
  if (desc == NULL) {
    // Based on Level 0 header file,
    // zeEventPoolCreate will return ZE_RESULT_ERROR_INVALID_NULL_POINTER for this caes.
    // Therefore, we do nothing in this case.
    return;
  }

  // Here we need to allocate a new event pool descriptor
  // as we cannot directly change the passed in object (declared ad const)
  // This leads to one description per event pool creation.
  pool_desc->flags = desc->flags;
  pool_desc->count = desc->count;

  // We attach the time stamp flag to the event pool,
  // so that we can query time stamps for events in this pool.
  int flags = pool_desc->flags | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
  pool_desc->flags = (ze_event_pool_flag_t)(flags);
}

static ze_event_handle_t
level0_command_list_append_launch_kernel_entry
(
  ze_kernel_handle_t kernel,
  ze_command_list_handle_t command_list,
  ze_event_handle_t event
)
{
  ze_event_pool_handle_t event_pool = NULL;

  if (event == NULL) {
    ze_context_handle_t hContext = level0_commandlist_context_map_lookup(command_list);
    // If the kernel is launched without an event,
    // we create a new event for collecting time stamps
    level0_create_new_event(hContext, &event, &event_pool);
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
  return event;
}

static ze_event_handle_t
level0_command_list_append_launch_memcpy_entry
(
  ze_command_list_handle_t command_list,
  ze_event_handle_t event,
  size_t mem_copy_size,
  const void* dest_ptr,
  const void* src_ptr
)
{
  ze_event_pool_handle_t event_pool = NULL;
  ze_context_handle_t hContext = level0_commandlist_context_map_lookup(command_list);
  if (event == NULL) {
    // If the memcpy is launched without an event,
    // we create a new event for collecting time stamps
    level0_create_new_event(hContext, &event, &event_pool);
  }

  ze_memory_type_t src_type = ZE_MEMORY_TYPE_HOST;
  ze_memory_type_t dst_type = ZE_MEMORY_TYPE_HOST;
  level0_get_memory_types(hContext, src_ptr, dest_ptr, &src_type, &dst_type);

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
  return event;
}


static void
level0_command_list_create_exit
(
  ze_command_list_handle_t handle,
  ze_context_handle_t hContext,
  int isImmediateList
)
{
  PRINT("level0_command_list_create_exit: command list %p, context handle %p, imm list %d\n",
    (void*)handle, (void*)hContext, isImmediateList);
  // Record the creation of a command list
  // command list map: command list handle -> a list of kernel launchs and memcpy
  if (!isImmediateList) {
    level0_commandlist_map_insert(handle);
  }
  // command list context map: command list handle -> context handle
  level0_commandlist_context_map_insert(handle, hContext);
}

static void
level0_command_list_destroy_entry
(
  ze_command_list_handle_t handle
)
{
  level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(handle);
  level0_commandlist_context_map_delete(handle);

  // If this happens, it is an immedicate list
  if (command_list_head == NULL) {
    return;
  }

  level0_data_node_t * command_node = *command_list_head;
  for (; command_node != NULL; command_node = command_node->next) {
    level0_attribute_event(command_node->event);
  }

  // Record the deletion of a command list
  level0_commandlist_map_delete(handle);
}

static void
level0_command_queue_execute_command_list_entry
(
  uint32_t numCommandLists,                       ///< [in] number of command lists to execute
  ze_command_list_handle_t* phCommandLists       ///< [in][range(0, numCommandLists)] list of handles of the command lists
)
{
  // We associate GPU metrics for GPU activitities in non-immediate command list
  // to the CPU call contexts where the command list is executed, not where
  // the GPU activity is appended.
  uint32_t i;
  for (i = 0; i < numCommandLists; ++i) {
    ze_command_list_handle_t command_list_handle = phCommandLists[i];
    PRINT("level0_command_queue_execute_command_list_entry: command list %p\n", (void*)command_list_handle);
    level0_data_node_t ** command_list_head = level0_commandlist_map_lookup(command_list_handle);
    level0_data_node_t * command_node = *command_list_head;
    for (; command_node != NULL; command_node = command_node->next) {
      level0_command_begin(command_node);
    }
  }
}

static void
level0_process_immediate_command_list
(
  ze_event_handle_t event,
  ze_command_list_handle_t command_list
)
{
  level0_data_node_t ** command_list_data_head = level0_commandlist_map_lookup(command_list);
  if (command_list_data_head == NULL) {
    // This is a GPU activity to an immediate command list
    level0_data_node_t* data_for_act = level0_event_map_lookup(event);
    level0_attribute_event(event);

    // For command in immediate command list,
    // the ownership of data node belongs to the user, not the command list
    level0_data_node_return_free_list(data_for_act);
  }
}

//******************************************************************************
// L0 public API override
//******************************************************************************

ze_result_t
zeInit
(
  ze_init_flag_t flag
)
{
  // Entry action
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeInit,(flag));
  level0_check_result(ret, __LINE__);
  // Exit action
  get_gpu_driver_and_device();
  return ret;
}

ze_result_t
zeCommandListAppendLaunchKernel
(
  ze_command_list_handle_t hCommandList,          ///< [in] handle of the command list
  ze_kernel_handle_t hKernel,                     ///< [in] handle of the kernel object
  const ze_group_count_t* pLaunchFuncArgs,        ///< [in] thread group launch arguments
  ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
  uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                  ///< if `nullptr == phWaitEvents`
  ze_event_handle_t* phWaitEvents                 ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                  ///< on before launching
)
{
  PRINT("Enter zeCommandListAppendLaunchKernel wrapper\n");
  // Entry action:
  // We need to create a new event for querying time stamps
  // if the user appends the kernel with an empty event parameter
  ze_event_handle_t new_event_handle = level0_command_list_append_launch_kernel_entry(
    hKernel, hCommandList, hSignalEvent);

  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandListAppendLaunchKernel,
    (hCommandList, hKernel, pLaunchFuncArgs,
    new_event_handle, numWaitEvents, phWaitEvents));

  // Exit action
  level0_process_immediate_command_list(new_event_handle, hCommandList);
  return ret;
}

ze_result_t
zeCommandListAppendMemoryCopy
(
  ze_command_list_handle_t hCommandList,          ///< [in] handle of command list
  void* dstptr,                                   ///< [in] pointer to destination memory to copy to
  const void* srcptr,                             ///< [in] pointer to source memory to copy from
  size_t size,                                    ///< [in] size in bytes to copy
  ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
  uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                  ///< if `nullptr == phWaitEvents`
  ze_event_handle_t* phWaitEvents                 ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                  ///< on before launching
)
{
  PRINT("Enter zeCommandListAppendMemoryCopy wrapper\n");
  // Entry action:
  // We need to create a new event for querying time stamps
  // if the user appends the kernel with an empty event parameter
  ze_event_handle_t new_event_handle =
  level0_command_list_append_launch_memcpy_entry(
      hCommandList, hSignalEvent, size, dstptr, srcptr);
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandListAppendMemoryCopy,
    (hCommandList, dstptr, srcptr, size, new_event_handle, numWaitEvents, phWaitEvents));

  // Exit action
  level0_process_immediate_command_list(new_event_handle, hCommandList);
  return ret;
}


ze_result_t
zeCommandListCreate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_list_desc_t* desc,             ///< [in] pointer to command list descriptor
  ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
)
{
  // Entry action
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandListCreate,
    (hContext, hDevice, desc, phCommandList));

  // Exit action
  level0_command_list_create_exit(*phCommandList, hContext, 0);
  return ret;
}

ze_result_t
zeCommandListCreateImmediate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_queue_desc_t* altdesc,         ///< [in] pointer to command queue descriptor
  ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
)
{
  // Entry action
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandListCreateImmediate,
    (hContext, hDevice, altdesc, phCommandList));

  // Exit action
  level0_command_list_create_exit(*phCommandList, hContext, 1);
  return ret;
}

ze_result_t
zeCommandListDestroy
(
  ze_command_list_handle_t hCommandList           ///< [in][release] handle of command list object to destroy
)
{
  // Entry action
  level0_command_list_destroy_entry(hCommandList);
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandListDestroy, (hCommandList));
  // Exit action
  return ret;
}

ze_result_t
zeCommandQueueExecuteCommandLists
(
  ze_command_queue_handle_t hCommandQueue,        ///< [in] handle of the command queue
  uint32_t numCommandLists,                       ///< [in] number of command lists to execute
  ze_command_list_handle_t* phCommandLists,       ///< [in][range(0, numCommandLists)] list of handles of the command lists
                                                  ///< to execute
  ze_fence_handle_t hFence                        ///< [in][optional] handle of the fence to signal on completion
)
{
  PRINT("Enter zeCommandQueueExecuteCommandLists wrapper\n");
  // Entry action
  level0_command_queue_execute_command_list_entry(numCommandLists, phCommandLists);
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeCommandQueueExecuteCommandLists,
    (hCommandQueue, numCommandLists, phCommandLists, hFence));
  // Exit action
  return ret;
}

ze_result_t
zeEventPoolCreate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  const ze_event_pool_desc_t* desc,               ///< [in] pointer to event pool descriptor
  uint32_t numDevices,                            ///< [in][optional] number of device handles; must be 0 if `nullptr ==
                                                  ///< phDevices`
  ze_device_handle_t* phDevices,                  ///< [in][optional][range(0, numDevices)] array of device handles which
                                                  ///< have visibility to the event pool.
                                                  ///< if nullptr, then event pool is visible to all devices supported by the
                                                  ///< driver instance.
  ze_event_pool_handle_t* phEventPool             ///< [out] pointer handle of event pool object created
)
{
  // Entry action
  ze_event_pool_desc_t pool_desc;
  level0_event_pool_create_entry(desc, &pool_desc);
  // Execute the real level0 API
  ze_result_t ret;
  if (desc == NULL) {
    ret = HPCRUN_LEVEL0_CALL(zeEventPoolCreate,
      (hContext, NULL, numDevices, phDevices, phEventPool));
  } else {
    ret = HPCRUN_LEVEL0_CALL(zeEventPoolCreate,
      (hContext, &pool_desc, numDevices, phDevices, phEventPool));
  }
  // Exit action
  return ret;
}

ze_result_t
zeEventDestroy
(
  ze_event_handle_t hEvent                        ///< [in][release] handle of event object to destroy
)
{
  // Entry action
  level0_attribute_event(hEvent);
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeEventDestroy, (hEvent));
  // Exit action
  return ret;
}

ze_result_t
zeEventHostReset
(
  ze_event_handle_t hEvent                        ///< [in] handle of the event
)
{
  // Entry action
  level0_attribute_event(hEvent);
  // Execute the real level0 API
  ze_result_t ret = HPCRUN_LEVEL0_CALL(zeEventHostReset, (hEvent));

  // Exit action
  return ret;
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

}

void
level0_fini
(
 void* args
)
{
  gpu_application_thread_process_activities();
}