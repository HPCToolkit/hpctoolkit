// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_NVIDIA_PRIVATE_H
#define HPCRUN_FOIL_NVIDIA_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "common.h"

#include <cuda.h>
#include <cupti.h>

struct hpcrun_foil_appdispatch_nvidia {
  CUptiResult (*cuptiActivityEnable)(CUpti_ActivityKind kind);
  CUptiResult (*cuptiActivityDisable)(CUpti_ActivityKind kind);
  CUptiResult (*cuptiActivityEnableContext)(CUcontext context, CUpti_ActivityKind kind);
  CUptiResult (*cuptiActivityDisableContext)(CUcontext context,
                                             CUpti_ActivityKind kind);
  CUptiResult (*cuptiActivityConfigurePCSampling)(
      CUcontext ctx, CUpti_ActivityPCSamplingConfig* config);
  CUptiResult (*cuptiActivityRegisterCallbacks)(
      CUpti_BuffersCallbackRequestFunc funcBufferRequested,
      CUpti_BuffersCallbackCompleteFunc funcBufferCompleted);
  CUptiResult (*cuptiActivityPushExternalCorrelationId)(
      CUpti_ExternalCorrelationKind kind, uint64_t id);
  CUptiResult (*cuptiActivityPopExternalCorrelationId)(
      CUpti_ExternalCorrelationKind kind, uint64_t* lastId);
  CUptiResult (*cuptiActivityGetNextRecord)(uint8_t* buffer,
                                            size_t validBufferSizeBytes,
                                            CUpti_Activity** record);
  CUptiResult (*cuptiActivityGetNumDroppedRecords)(CUcontext context, uint32_t streamId,
                                                   size_t* dropped);
  CUptiResult (*cuptiActivitySetAttribute)(CUpti_ActivityAttribute attribute,
                                           size_t* value_size, void* value);
  CUptiResult (*cuptiActivityFlushAll)(uint32_t flag);
  CUptiResult (*cuptiGetTimestamp)(uint64_t* timestamp);
  CUptiResult (*cuptiEnableDomain)(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                   CUpti_CallbackDomain domain);
  CUptiResult (*cuptiFinalize)();
  CUptiResult (*cuptiGetResultString)(CUptiResult result, const char** str);
  CUptiResult (*cuptiSubscribe)(CUpti_SubscriberHandle* subscriber,
                                CUpti_CallbackFunc callback, void* userdata);
  CUptiResult (*cuptiEnableCallback)(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                     CUpti_CallbackDomain domain,
                                     CUpti_CallbackId cbid);
  CUptiResult (*cuptiUnsubscribe)(CUpti_SubscriberHandle subscriber);
  CUresult (*cuDeviceGetAttribute)(int* pi, CUdevice_attribute attrib, CUdevice dev);
  CUresult (*cuCtxGetCurrent)(CUcontext* ctx);
  CUresult (*cuFuncGetModule)(CUmodule* hmod, CUfunction function);
  CUresult (*cuDriverGetVersion)(int* version);
  cudaError_t (*cudaGetDevice)(int* device_id);
  cudaError_t (*cudaRuntimeGetVersion)(int* runtimeVersion);
  cudaError_t (*cudaDeviceSynchronize)();
  cudaError_t (*cudaMemcpy)(void* dst, const void* src, size_t count,
                            enum cudaMemcpyKind kind);
};

HPCRUN_EXPOSED_API const struct hpcrun_foil_appdispatch_nvidia hpcrun_dispatch_nvidia;

#endif // HPCRUN_FOIL_NVIDIA_PRIVATE_H
