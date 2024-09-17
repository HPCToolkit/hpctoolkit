// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "nvidia-private.h"

const struct hpcrun_foil_appdispatch_nvidia hpcrun_dispatch_nvidia = {
    .cuptiActivityEnable = &cuptiActivityEnable,
    .cuptiActivityDisable = &cuptiActivityDisable,
    .cuptiActivityEnableContext = &cuptiActivityEnableContext,
    .cuptiActivityDisableContext = &cuptiActivityDisableContext,
    .cuptiActivityConfigurePCSampling = &cuptiActivityConfigurePCSampling,
    .cuptiActivityRegisterCallbacks = &cuptiActivityRegisterCallbacks,
    .cuptiActivityPushExternalCorrelationId = &cuptiActivityPushExternalCorrelationId,
    .cuptiActivityPopExternalCorrelationId = &cuptiActivityPopExternalCorrelationId,
    .cuptiActivityGetNextRecord = &cuptiActivityGetNextRecord,
    .cuptiActivityGetNumDroppedRecords = &cuptiActivityGetNumDroppedRecords,
    .cuptiActivitySetAttribute = &cuptiActivitySetAttribute,
    .cuptiActivityFlushAll = &cuptiActivityFlushAll,
    .cuptiGetTimestamp = &cuptiGetTimestamp,
    .cuptiEnableDomain = &cuptiEnableDomain,
    .cuptiFinalize = &cuptiFinalize,
    .cuptiGetResultString = &cuptiGetResultString,
    .cuptiSubscribe = &cuptiSubscribe,
    .cuptiEnableCallback = &cuptiEnableCallback,
    .cuptiUnsubscribe = &cuptiUnsubscribe,
    .cuDeviceGetAttribute = &cuDeviceGetAttribute,
    .cuCtxGetCurrent = &cuCtxGetCurrent,
    .cuFuncGetModule = &cuFuncGetModule,
    .cuDriverGetVersion = &cuDriverGetVersion,
    .cudaGetDevice = &cudaGetDevice,
    .cudaRuntimeGetVersion = &cudaRuntimeGetVersion,
    .cudaDeviceSynchronize = &cudaDeviceSynchronize,
    .cudaMemcpy = &cudaMemcpy,
};
