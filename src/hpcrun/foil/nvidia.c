// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "nvidia.h"

#include "nvidia-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

static const struct hpcrun_foil_appdispatch_nvidia* dispatch_var = NULL;

static void init_dispatch() {
  void* handle =
      dlmopen(LM_ID_BASE, "libhpcrun_dlopen_nvidia.so", RTLD_NOW | RTLD_DEEPBIND);
  if (handle == NULL) {
    assert(false && "Failed to load foil_nvidia.so");
    abort();
  }
  dispatch_var = dlsym(handle, "hpcrun_dispatch_nvidia");
  if (dispatch_var == NULL) {
    assert(false && "Failed to fetch dispatch from foil_nvidia.so");
    abort();
  }
}

static const struct hpcrun_foil_appdispatch_nvidia* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return dispatch_var;
}

CUptiResult f_cuptiActivityEnable(CUpti_ActivityKind kind) {
  return dispatch()->cuptiActivityEnable(kind);
}

CUptiResult f_cuptiActivityDisable(CUpti_ActivityKind kind) {
  return dispatch()->cuptiActivityDisable(kind);
}

CUptiResult f_cuptiActivityEnableContext(CUcontext context, CUpti_ActivityKind kind) {
  return dispatch()->cuptiActivityEnableContext(context, kind);
}

CUptiResult f_cuptiActivityDisableContext(CUcontext context, CUpti_ActivityKind kind) {
  return dispatch()->cuptiActivityDisableContext(context, kind);
}

CUptiResult f_cuptiActivityConfigurePCSampling(CUcontext ctx,
                                               CUpti_ActivityPCSamplingConfig* config) {
  return dispatch()->cuptiActivityConfigurePCSampling(ctx, config);
}

CUptiResult f_cuptiActivityRegisterCallbacks(
    CUpti_BuffersCallbackRequestFunc funcBufferRequested,
    CUpti_BuffersCallbackCompleteFunc funcBufferCompleted) {
  return dispatch()->cuptiActivityRegisterCallbacks(funcBufferRequested,
                                                    funcBufferCompleted);
}

CUptiResult f_cuptiActivityPushExternalCorrelationId(CUpti_ExternalCorrelationKind kind,
                                                     uint64_t id) {
  return dispatch()->cuptiActivityPushExternalCorrelationId(kind, id);
}

CUptiResult f_cuptiActivityPopExternalCorrelationId(CUpti_ExternalCorrelationKind kind,
                                                    uint64_t* lastId) {
  return dispatch()->cuptiActivityPopExternalCorrelationId(kind, lastId);
}

CUptiResult f_cuptiActivityGetNextRecord(uint8_t* buffer, size_t validBufferSizeBytes,
                                         CUpti_Activity** record) {
  return dispatch()->cuptiActivityGetNextRecord(buffer, validBufferSizeBytes, record);
}

CUptiResult f_cuptiActivityGetNumDroppedRecords(CUcontext context, uint32_t streamId,
                                                size_t* dropped) {
  return dispatch()->cuptiActivityGetNumDroppedRecords(context, streamId, dropped);
}

CUptiResult f_cuptiActivitySetAttribute(CUpti_ActivityAttribute attribute,
                                        size_t* value_size, void* value) {
  return dispatch()->cuptiActivitySetAttribute(attribute, value_size, value);
}

CUptiResult f_cuptiActivityFlushAll(uint32_t flag) {
  return dispatch()->cuptiActivityFlushAll(flag);
}

CUptiResult f_cuptiGetTimestamp(uint64_t* timestamp) {
  return dispatch()->cuptiGetTimestamp(timestamp);
}

CUptiResult f_cuptiEnableDomain(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                CUpti_CallbackDomain domain) {
  return dispatch()->cuptiEnableDomain(enable, subscriber, domain);
}

CUptiResult f_cuptiFinalize() { return dispatch()->cuptiFinalize(); }

CUptiResult f_cuptiGetResultString(CUptiResult result, const char** str) {
  return dispatch()->cuptiGetResultString(result, str);
}

CUptiResult f_cuptiSubscribe(CUpti_SubscriberHandle* subscriber,
                             CUpti_CallbackFunc callback, void* userdata) {
  return dispatch()->cuptiSubscribe(subscriber, callback, userdata);
}

CUptiResult f_cuptiEnableCallback(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                  CUpti_CallbackDomain domain, CUpti_CallbackId cbid) {
  return dispatch()->cuptiEnableCallback(enable, subscriber, domain, cbid);
}

CUptiResult f_cuptiUnsubscribe(CUpti_SubscriberHandle subscriber) {
  return dispatch()->cuptiUnsubscribe(subscriber);
}

CUresult f_cuDeviceGetAttribute(int* pi, CUdevice_attribute attrib, CUdevice dev) {
  return dispatch()->cuDeviceGetAttribute(pi, attrib, dev);
}

CUresult f_cuCtxGetCurrent(CUcontext* ctx) { return dispatch()->cuCtxGetCurrent(ctx); }

CUresult f_cuFuncGetModule(CUmodule* hmod, CUfunction function) {
  return dispatch()->cuFuncGetModule(hmod, function);
}

CUresult f_cuDriverGetVersion(int* version) {
  return dispatch()->cuDriverGetVersion(version);
}

cudaError_t f_cudaGetDevice(int* device_id) {
  return dispatch()->cudaGetDevice(device_id);
}

cudaError_t f_cudaRuntimeGetVersion(int* runtimeVersion) {
  return dispatch()->cudaRuntimeGetVersion(runtimeVersion);
}

cudaError_t f_cudaDeviceSynchronize() { return dispatch()->cudaDeviceSynchronize(); }

cudaError_t f_cudaMemcpy(void* dst, const void* src, size_t count,
                         enum cudaMemcpyKind kind) {
  return dispatch()->cudaMemcpy(dst, src, count, kind);
}
