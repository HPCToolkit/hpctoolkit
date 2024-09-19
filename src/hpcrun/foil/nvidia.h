// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_NVIDIA_H
#define HPCRUN_FOIL_NVIDIA_H

#include <cuda.h>
#include <cupti.h>

#ifdef __cplusplus
extern "C" {
#endif

CUptiResult f_cuptiActivityEnable(CUpti_ActivityKind kind);
CUptiResult f_cuptiActivityDisable(CUpti_ActivityKind kind);
CUptiResult f_cuptiActivityEnableContext(CUcontext context, CUpti_ActivityKind kind);
CUptiResult f_cuptiActivityDisableContext(CUcontext context, CUpti_ActivityKind kind);
CUptiResult f_cuptiActivityConfigurePCSampling(CUcontext ctx,
                                               CUpti_ActivityPCSamplingConfig* config);
CUptiResult
f_cuptiActivityRegisterCallbacks(CUpti_BuffersCallbackRequestFunc funcBufferRequested,
                                 CUpti_BuffersCallbackCompleteFunc funcBufferCompleted);
CUptiResult f_cuptiActivityPushExternalCorrelationId(CUpti_ExternalCorrelationKind kind,
                                                     uint64_t id);
CUptiResult f_cuptiActivityPopExternalCorrelationId(CUpti_ExternalCorrelationKind kind,
                                                    uint64_t* lastId);
CUptiResult f_cuptiActivityGetNextRecord(uint8_t* buffer, size_t validBufferSizeBytes,
                                         CUpti_Activity** record);
CUptiResult f_cuptiActivityGetNumDroppedRecords(CUcontext context, uint32_t streamId,
                                                size_t* dropped);
CUptiResult f_cuptiActivitySetAttribute(CUpti_ActivityAttribute attribute,
                                        size_t* value_size, void* value);
CUptiResult f_cuptiActivityFlushAll(uint32_t flag);
CUptiResult f_cuptiGetTimestamp(uint64_t* timestamp);
CUptiResult f_cuptiGetTimestamp(uint64_t* timestamp);
CUptiResult f_cuptiEnableDomain(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                CUpti_CallbackDomain domain);
CUptiResult f_cuptiFinalize();
CUptiResult f_cuptiGetResultString(CUptiResult result, const char** str);
CUptiResult f_cuptiSubscribe(CUpti_SubscriberHandle* subscriber,
                             CUpti_CallbackFunc callback, void* userdata);
CUptiResult f_cuptiEnableCallback(uint32_t enable, CUpti_SubscriberHandle subscriber,
                                  CUpti_CallbackDomain domain, CUpti_CallbackId cbid);
CUptiResult f_cuptiUnsubscribe(CUpti_SubscriberHandle subscriber);
CUresult f_cuDeviceGetAttribute(int* pi, CUdevice_attribute attrib, CUdevice dev);
CUresult f_cuCtxGetCurrent(CUcontext* ctx);
CUresult f_cuFuncGetModule(CUmodule* hmod, CUfunction function);
CUresult f_cuDriverGetVersion(int* version);
cudaError_t f_cudaGetDevice(int* device_id);
cudaError_t f_cudaRuntimeGetVersion(int* runtimeVersion);
cudaError_t f_cudaDeviceSynchronize();
cudaError_t f_cudaMemcpy(void* dst, const void* src, size_t count,
                         enum cudaMemcpyKind kind);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_NVIDIA_H
