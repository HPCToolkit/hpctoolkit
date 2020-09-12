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
#include "level0-intercept.h"

#include <hpcrun/main.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>

#include <level_zero/ze_api.h>

//******************************************************************************
// macros
//******************************************************************************
#define DEBUG 1

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
  macro(zeEventGetTimestamp) \
  macro(zeDriverGetMemAllocProperties)


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
  level0_intercept_setup();
  HPCRUN_LEVEL0_CALL(zeInit,(ZE_INIT_FLAG_NONE));
  get_gpu_driver_and_device();
}

void
level0_fini
(
 void* args
)
{
  gpu_application_thread_process_activities();
  level0_intercept_teardown();
}

void
level0_create_new_event
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

void
level0_attribute_event
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

void
level0_get_memory_types
(
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
  if (HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, src_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    *src_type_ptr = property.type;
  }
  if (HPCRUN_LEVEL0_CALL(zeDriverGetMemAllocProperties, (hDriver, dest_ptr, &property, NULL)) == ZE_RESULT_SUCCESS) {
    *dst_type_ptr = property.type;
  }
}