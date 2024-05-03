// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

//******************************************************************************
// local includes
//******************************************************************************

#include "foil.h"
#include "../gpu/api/intel/level0/level0-api.h"


//******************************************************************************
// L0 public API override
//******************************************************************************

HPCRUN_EXPOSED ze_result_t
zeInit
(
  ze_init_flag_t flag
)
{
  LOOKUP_FOIL_BASE(base, zeInit);
  return base(flag);
}

HPCRUN_EXPOSED ze_result_t
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
  LOOKUP_FOIL_BASE(base, zeCommandListAppendLaunchKernel);
  return base(
    hCommandList, hKernel, pLaunchFuncArgs,
    hSignalEvent, numWaitEvents, phWaitEvents);
}

HPCRUN_EXPOSED ze_result_t
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
  LOOKUP_FOIL_BASE(base, zeCommandListAppendMemoryCopy);
  return base(
    hCommandList, dstptr, srcptr, size,
    hSignalEvent, numWaitEvents, phWaitEvents);
}

HPCRUN_EXPOSED ze_result_t
zeCommandListCreate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_list_desc_t* desc,             ///< [in] pointer to command list descriptor
  ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
)
{
  LOOKUP_FOIL_BASE(base, zeCommandListCreate);
  return base(
    hContext, hDevice, desc, phCommandList);
}

HPCRUN_EXPOSED ze_result_t
zeCommandListCreateImmediate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_queue_desc_t* altdesc,         ///< [in] pointer to command queue descriptor
  ze_command_list_handle_t* phCommandList         ///< [out] pointer to handle of command list object created
)
{
  LOOKUP_FOIL_BASE(base, zeCommandListCreateImmediate);
  return base(
    hContext, hDevice, altdesc, phCommandList);
}

HPCRUN_EXPOSED ze_result_t
zeCommandListDestroy
(
  ze_command_list_handle_t hCommandList           ///< [in][release] handle of command list object to destroy
)
{
  LOOKUP_FOIL_BASE(base, zeCommandListDestroy);
  return base(hCommandList);
}

HPCRUN_EXPOSED ze_result_t
zeCommandListReset
(
  ze_command_list_handle_t hCommandList           ///< [in] handle of command list object to reset
)
{
  LOOKUP_FOIL_BASE(base, zeCommandListReset);
  return base(hCommandList);
}

HPCRUN_EXPOSED ze_result_t
zeCommandQueueExecuteCommandLists
(
  ze_command_queue_handle_t hCommandQueue,        ///< [in] handle of the command queue
  uint32_t numCommandLists,                       ///< [in] number of command lists to execute
  ze_command_list_handle_t* phCommandLists,       ///< [in][range(0, numCommandLists)] list of handles of the command lists
                                                  ///< to execute
  ze_fence_handle_t hFence                        ///< [in][optional] handle of the fence to signal on completion
)
{
  LOOKUP_FOIL_BASE(base, zeCommandQueueExecuteCommandLists);
  return base(
    hCommandQueue, numCommandLists, phCommandLists, hFence);
}

HPCRUN_EXPOSED ze_result_t
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
  LOOKUP_FOIL_BASE(base, zeEventPoolCreate);
  return base(
    hContext, desc, numDevices, phDevices, phEventPool);
}

HPCRUN_EXPOSED ze_result_t
zeEventDestroy
(
  ze_event_handle_t hEvent                        ///< [in][release] handle of event object to destroy
)
{
  LOOKUP_FOIL_BASE(base, zeEventDestroy)
  return base(hEvent);
}

HPCRUN_EXPOSED ze_result_t
zeEventHostReset
(
  ze_event_handle_t hEvent                        ///< [in] handle of the event
)
{
  LOOKUP_FOIL_BASE(base, zeEventHostReset);
  return base(hEvent);
}

HPCRUN_EXPOSED ze_result_t
zeModuleCreate
(
  ze_context_handle_t hContext,                // [in] handle of the context object
  ze_device_handle_t hDevice,                  // [in] handle of the device
  const ze_module_desc_t *desc,                // [in] pointer to module descriptor
  ze_module_handle_t *phModule,                // [out] pointer to handle of module object created
  ze_module_build_log_handle_t *phBuildLog     // [out][optional] pointer to handle of module’s build log.
)
{
  LOOKUP_FOIL_BASE(base, zeModuleCreate);
  return base(
    hContext, hDevice, desc, phModule, phBuildLog
  );
}

HPCRUN_EXPOSED ze_result_t
zeModuleDestroy
(
  ze_module_handle_t hModule       // [in][release] handle of the module
)
{
  LOOKUP_FOIL_BASE(base, zeModuleDestroy);
  return base(hModule);
}

HPCRUN_EXPOSED ze_result_t
zeKernelCreate
(
  ze_module_handle_t hModule,          // [in] handle of the module
  const ze_kernel_desc_t *desc,        // [in] pointer to kernel descriptor
  ze_kernel_handle_t *phKernel         // [out] handle of the Function object
)
{
  LOOKUP_FOIL_BASE(base, zeKernelCreate);
  return base(
    hModule, desc, phKernel
  );
}

HPCRUN_EXPOSED ze_result_t
zeKernelDestroy
(
  ze_kernel_handle_t hKernel      // [in][release] handle of the kernel object
)
{
  LOOKUP_FOIL_BASE(base, zeKernelDestroy);
  return base(hKernel);
}

HPCRUN_EXPOSED ze_result_t
zeFenceDestroy
(
  ze_fence_handle_t hFence        // [in][release] handle of fence object to destroy
)
{
  LOOKUP_FOIL_BASE(base, zeFenceDestroy);
  return base(hFence);
}

HPCRUN_EXPOSED ze_result_t
zeFenceReset
(
  ze_fence_handle_t hFence       //  [in] handle of the fence
)
{
  LOOKUP_FOIL_BASE(base, zeFenceReset);
  return base(hFence);
}

HPCRUN_EXPOSED ze_result_t
zeCommandQueueSynchronize
(
  ze_command_queue_handle_t hCommandQueue,   // [in] handle of the command queue
  uint64_t timeout                           // [in] if non-zero, then indicates the maximum time (in nanoseconds) to yield before returning
)
{
  LOOKUP_FOIL_BASE(base, zeCommandQueueSynchronize);
  return base(hCommandQueue, timeout);
}
