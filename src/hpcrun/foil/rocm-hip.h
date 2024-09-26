// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCM_HIP_H
#define HPCRUN_FOIL_ROCM_HIP_H

#include <hip/hip_runtime.h>

hipError_t f_hipDeviceSynchronize();
hipError_t f_hipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attrib, int dev);
hipError_t f_hipStreamSynchronize(hipStream_t stream);

#endif // HPCRUN_FOIL_ROCM_HIP_H
