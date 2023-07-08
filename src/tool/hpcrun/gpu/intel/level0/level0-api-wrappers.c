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
// Copyright ((c)) 2002-2023, Rice University
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


#include "level0-api.h"

//******************************************************************************
// L0 public API override
//******************************************************************************

ze_result_t
zeInit
(
  ze_init_flag_t flag
)
{
  return hpcrun_zeInit(flag);
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
  return hpcrun_zeCommandListAppendLaunchKernel(
    hCommandList, hKernel, pLaunchFuncArgs,
    hSignalEvent, numWaitEvents, phWaitEvents);
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
  return hpcrun_zeCommandListAppendMemoryCopy(
    hCommandList, dstptr, srcptr, size,
    hSignalEvent, numWaitEvents, phWaitEvents);
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
  return hpcrun_zeCommandListCreate(
    hContext, hDevice, desc, phCommandList);
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
  return hpcrun_zeCommandListCreateImmediate(
    hContext, hDevice, altdesc, phCommandList);
}

ze_result_t
zeCommandListDestroy
(
  ze_command_list_handle_t hCommandList           ///< [in][release] handle of command list object to destroy
)
{
  return hpcrun_zeCommandListDestroy(hCommandList);
}


ze_result_t
zeCommandListReset
(
  ze_command_list_handle_t hCommandList           ///< [in] handle of command list object to reset
)
{
  return hpcrun_zeCommandListReset(hCommandList);
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
  return hpcrun_zeCommandQueueExecuteCommandLists(
    hCommandQueue, numCommandLists, phCommandLists, hFence);
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
  return hpcrun_zeEventPoolCreate(
    hContext, desc, numDevices, phDevices, phEventPool);
}

ze_result_t
zeEventDestroy
(
  ze_event_handle_t hEvent                        ///< [in][release] handle of event object to destroy
)
{
  return hpcrun_zeEventDestroy(hEvent);
}

ze_result_t
zeEventHostReset
(
  ze_event_handle_t hEvent                        ///< [in] handle of the event
)
{
  return hpcrun_zeEventHostReset(hEvent);
}

ze_result_t
zeModuleCreate
(
  ze_context_handle_t hContext,                // [in] handle of the context object
  ze_device_handle_t hDevice,                  // [in] handle of the device
  const ze_module_desc_t *desc,                // [in] pointer to module descriptor
  ze_module_handle_t *phModule,                // [out] pointer to handle of module object created
  ze_module_build_log_handle_t *phBuildLog     // [out][optional] pointer to handle of module’s build log.
)
{
  return hpcrun_zeModuleCreate(
    hContext, hDevice, desc, phModule, phBuildLog
  );
}

ze_result_t
zeModuleDestroy
(
  ze_module_handle_t hModule       // [in][release] handle of the module
)
{
  return hpcrun_zeModuleDestroy(hModule);
}

ze_result_t
zeKernelCreate
(
  ze_module_handle_t hModule,          // [in] handle of the module
  const ze_kernel_desc_t *desc,        // [in] pointer to kernel descriptor
  ze_kernel_handle_t *phKernel         // [out] handle of the Function object
)
{
  return hpcrun_zeKernelCreate(
    hModule, desc, phKernel
  );
}

ze_result_t
zeKernelDestroy
(
  ze_kernel_handle_t hKernel      // [in][release] handle of the kernel object
)
{
  return hpcrun_zeKernelDestroy(hKernel);
}

ze_result_t
zeFenceDestroy
(
  ze_fence_handle_t hFence        // [in][release] handle of fence object to destroy
)
{
  return hpcrun_zeFenceDestroy(hFence);
}

ze_result_t
zeFenceReset
(
  ze_fence_handle_t hFence       //  [in] handle of the fence
)
{
  return hpcrun_zeFenceReset(hFence);
}

ze_result_t
zeCommandQueueSynchronize
(
  ze_command_queue_handle_t hCommandQueue,   // [in] handle of the command queue
  uint64_t timeout                           // [in] if non-zero, then indicates the maximum time (in nanoseconds) to yield before returning
)
{
  return hpcrun_zeCommandQueueSynchronize(hCommandQueue, timeout);
}
