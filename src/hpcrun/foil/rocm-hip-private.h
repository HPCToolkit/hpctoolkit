// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCM_HIP_PRIVATE_H
#define HPCRUN_FOIL_ROCM_HIP_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "common.h"
#include "rocm-hip.h"

struct hpcrun_foil_appdispatch_rocm_hip {
  hipError_t (*hipDeviceSynchronize)();
  hipError_t (*hipDeviceGetAttribute)(int* pi, hipDeviceAttribute_t attrib, int dev);
  hipError_t (*hipStreamSynchronize)(hipStream_t stream);
};

HPCRUN_EXPOSED_API const struct hpcrun_foil_appdispatch_rocm_hip
    hpcrun_dispatch_rocm_hip;

#endif // HPCRUN_FOIL_ROCM_HIP_PRIVATE_H
