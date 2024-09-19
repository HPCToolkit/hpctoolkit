// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_api_h
#define level0_api_h

//******************************************************************************
// global includes
//******************************************************************************

#include <stdint.h>
#include <stdbool.h>


//******************************************************************************
// local includes
//******************************************************************************

#include <ze_api.h>
#include <zet_api.h>

#include "../../common/gpu-instrumentation.h"
#include "../../../../foil/level0.h"



//******************************************************************************
// global variables
//******************************************************************************



//******************************************************************************
// interface operations
//******************************************************************************

ze_result_t
hpcrun_zeInit
(
  ze_init_flag_t flag,
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListAppendLaunchKernel
(
  ze_command_list_handle_t hCommandList,          ///< [in] handle of the command list
  ze_kernel_handle_t hKernel,                     ///< [in] handle of the kernel object
  const ze_group_count_t* pLaunchFuncArgs,        ///< [in] thread group launch arguments
  ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
  uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                  ///< if `nullptr == phWaitEvents`
  ze_event_handle_t* phWaitEvents,                ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                  ///< on before launching
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListAppendMemoryCopy
(
  ze_command_list_handle_t hCommandList,          ///< [in] handle of command list
  void* dstptr,                                   ///< [in] pointer to destination memory to copy to
  const void* srcptr,                             ///< [in] pointer to source memory to copy from
  size_t size,                                    ///< [in] size in bytes to copy
  ze_event_handle_t hSignalEvent,                 ///< [in][optional] handle of the event to signal on completion
  uint32_t numWaitEvents,                         ///< [in][optional] number of events to wait on before launching; must be 0
                                                  ///< if `nullptr == phWaitEvents`
  ze_event_handle_t* phWaitEvents,                 ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                                  ///< on before launching
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListCreate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_list_desc_t* desc,             ///< [in] pointer to command list descriptor
  ze_command_list_handle_t* phCommandList,        ///< [out] pointer to handle of command list object created
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListCreateImmediate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  ze_device_handle_t hDevice,                     ///< [in] handle of the device object
  const ze_command_queue_desc_t* altdesc,         ///< [in] pointer to command queue descriptor
  ze_command_list_handle_t* phCommandList,        ///< [out] pointer to handle of command list object created
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListDestroy
(
  ze_command_list_handle_t hCommandList,          ///< [in][release] handle of command list object to destroy
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandListReset
(
  ze_command_list_handle_t hCommandList,          ///< [in] handle of command list object to reset
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandQueueExecuteCommandLists
(
  ze_command_queue_handle_t hCommandQueue,        ///< [in] handle of the command queue
  uint32_t numCommandLists,                       ///< [in] number of command lists to execute
  ze_command_list_handle_t* phCommandLists,       ///< [in][range(0, numCommandLists)] list of handles of the command lists
                                                  ///< to execute
  ze_fence_handle_t hFence,                       ///< [in][optional] handle of the fence to signal on completion
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeEventPoolCreate
(
  ze_context_handle_t hContext,                   ///< [in] handle of the context object
  const ze_event_pool_desc_t* desc,               ///< [in] pointer to event pool descriptor
  uint32_t numDevices,                            ///< [in][optional] number of device handles; must be 0 if `nullptr ==
                                                  ///< phDevices`
  ze_device_handle_t* phDevices,                  ///< [in][optional][range(0, numDevices)] array of device handles which
                                                  ///< have visibility to the event pool.
                                                  ///< if nullptr, then event pool is visible to all devices supported by the
                                                  ///< driver instance.
  ze_event_pool_handle_t* phEventPool,            ///< [out] pointer handle of event pool object created
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeEventDestroy
(
  ze_event_handle_t hEvent,                       ///< [in][release] handle of event object to destroy
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeEventHostReset
(
  ze_event_handle_t hEvent,                       ///< [in] handle of the event
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeModuleCreate
(
  ze_context_handle_t hContext,                // [in] handle of the context object
  ze_device_handle_t hDevice,                  // [in] handle of the device
  const ze_module_desc_t *desc,                // [in] pointer to module descriptor
  ze_module_handle_t *phModule,                // [out] pointer to handle of module object created
  ze_module_build_log_handle_t *phBuildLog,    // [out][optional] pointer to handle of module’s build log.
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeModuleDestroy
(
  ze_module_handle_t hModule,      // [in][release] handle of the module
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeKernelCreate
(
  ze_module_handle_t hModule,          // [in] handle of the module
  const ze_kernel_desc_t *desc,        // [in] pointer to kernel descriptor
  ze_kernel_handle_t *phKernel,        // [out] handle of the Function object
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeKernelDestroy
(
  ze_kernel_handle_t hKernel,     // [in][release] handle of the kernel object
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeFenceDestroy
(
  ze_fence_handle_t hFence,       // [in][release] handle of fence object to destroy
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeFenceReset
(
  ze_fence_handle_t hFence,      //  [in] handle of the fence
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

ze_result_t
hpcrun_zeCommandQueueSynchronize
(
  ze_command_queue_handle_t hCommandQueue,   // [in] handle of the command queue
  uint64_t timeout,                          // [in] if non-zero, then indicates the maximum time (in nanoseconds) to yield before returning
  const struct hpcrun_foil_appdispatch_level0* dispatch
);

void
level0_init
(
 gpu_instrumentation_t *inst_options
);

void
level0_fini
(
 void *args,
 int how
);

void
level0_flush
(
 void *args,
 int how
);

bool
level0_gtpin_enabled
(
  void
);

#endif
