// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "level0-private.h"
#include "level0.h"

#include <threads.h>

static struct hpcrun_foil_appdispatch_level0 dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_level0){
      .zeInit = foil_dlsym("zeInit"),
      .zeDriverGet = foil_dlsym("zeDriverGet"),
      .zeDeviceGet = foil_dlsym("zeDeviceGet"),
      .zeDeviceGetProperties = foil_dlsym("zeDeviceGetProperties"),
      .zeEventCreate = foil_dlsym("zeEventCreate"),
      .zeEventDestroy = foil_dlsym("zeEventDestroy"),
      .zeEventPoolCreate = foil_dlsym("zeEventPoolCreate"),
      .zeEventPoolDestroy = foil_dlsym("zeEventPoolDestroy"),
      .zeEventQueryStatus = foil_dlsym("zeEventQueryStatus"),
      .zeEventQueryKernelTimestamp = foil_dlsym("zeEventQueryKernelTimestamp"),
      .zeMemGetAllocProperties = foil_dlsym("zeMemGetAllocProperties"),
      .zeCommandListAppendLaunchKernel = foil_dlsym("zeCommandListAppendLaunchKernel"),
      .zeCommandListAppendMemoryCopy = foil_dlsym("zeCommandListAppendMemoryCopy"),
      .zeCommandListCreate = foil_dlsym("zeCommandListCreate"),
      .zeCommandListCreateImmediate = foil_dlsym("zeCommandListCreateImmediate"),
      .zeCommandListDestroy = foil_dlsym("zeCommandListDestroy"),
      .zeCommandListReset = foil_dlsym("zeCommandListReset"),
      .zeCommandQueueExecuteCommandLists =
          foil_dlsym("zeCommandQueueExecuteCommandLists"),
      .zeEventHostReset = foil_dlsym("zeEventHostReset"),
      .zeModuleCreate = foil_dlsym("zeModuleCreate"),
      .zeModuleDestroy = foil_dlsym("zeModuleDestroy"),
      .zeKernelCreate = foil_dlsym("zeKernelCreate"),
      .zeKernelDestroy = foil_dlsym("zeKernelDestroy"),
      .zeFenceDestroy = foil_dlsym("zeFenceDestroy"),
      .zeFenceReset = foil_dlsym("zeFenceReset"),
      .zeCommandQueueSynchronize = foil_dlsym("zeCommandQueueSynchronize"),
      .zeKernelGetName = foil_dlsym("zeKernelGetName"),
      .zetModuleGetDebugInfo = foil_dlsym("zetModuleGetDebugInfo"),
  };
}

static const struct hpcrun_foil_appdispatch_level0* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API ze_result_t zeInit(ze_init_flags_t flag) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeInit(flag, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t* pLaunchFuncArgs, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListAppendLaunchKernel(
      hCommandList, hKernel, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents,
      dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList, void* dstptr, const void* srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListAppendMemoryCopy(
      hCommandList, dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents,
      dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeCommandListCreate(
    ze_context_handle_t hContext, ze_device_handle_t hDevice,
    const ze_command_list_desc_t* desc, ze_command_list_handle_t* phCommandList) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListCreate(
      hContext, hDevice, desc, phCommandList, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeCommandListCreateImmediate(
    ze_context_handle_t hContext, ze_device_handle_t hDevice,
    const ze_command_queue_desc_t* altdesc, ze_command_list_handle_t* phCommandList) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListCreateImmediate(
      hContext, hDevice, altdesc, phCommandList, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t
zeCommandListDestroy(ze_command_list_handle_t hCommandList) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListDestroy(hCommandList,
                                                                   dispatch());
}

HPCRUN_EXPOSED_API ze_result_t
zeCommandListReset(ze_command_list_handle_t hCommandList) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandListReset(hCommandList,
                                                                 dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue, uint32_t numCommandLists,
    ze_command_list_handle_t* phCommandLists, ze_fence_handle_t hFence) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandQueueExecuteCommandLists(
      hCommandQueue, numCommandLists, phCommandLists, hFence, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeEventPoolCreate(ze_context_handle_t hContext,
                                                 const ze_event_pool_desc_t* desc,
                                                 uint32_t numDevices,
                                                 ze_device_handle_t* phDevices,
                                                 ze_event_pool_handle_t* phEventPool) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeEventPoolCreate(
      hContext, desc, numDevices, phDevices, phEventPool, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeEventDestroy(ze_event_handle_t hEvent) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeEventDestroy(hEvent, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeEventHostReset(ze_event_handle_t hEvent) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeEventHostReset(hEvent, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t
zeModuleCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice,
               const ze_module_desc_t* desc, ze_module_handle_t* phModule,
               ze_module_build_log_handle_t* phBuildLog) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeModuleCreate(
      hContext, hDevice, desc, phModule, phBuildLog, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeModuleDestroy(ze_module_handle_t hModule) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeModuleDestroy(hModule, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeKernelCreate(ze_module_handle_t hModule,
                                              const ze_kernel_desc_t* desc,
                                              ze_kernel_handle_t* phKernel) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeKernelCreate(hModule, desc, phKernel,
                                                             dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeKernelDestroy(ze_kernel_handle_t hKernel) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeKernelDestroy(hKernel, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeFenceDestroy(ze_fence_handle_t hFence) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeFenceDestroy(hFence, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t zeFenceReset(ze_fence_handle_t hFence) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeFenceReset(hFence, dispatch());
}

HPCRUN_EXPOSED_API ze_result_t
zeCommandQueueSynchronize(ze_command_queue_handle_t hCommandQueue, uint64_t timeout) {
  return hpcrun_foil_fetch_hooks_level0_dl()->zeCommandQueueSynchronize(
      hCommandQueue, timeout, dispatch());
}
