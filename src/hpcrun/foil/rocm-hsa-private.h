// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCM_HSA_PRIVATE_H
#define HPCRUN_FOIL_ROCM_HSA_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "common.h"
#include "rocm-hsa.h"

struct hpcrun_foil_appdispatch_rocm_hsa {
  hsa_status_t (*hsa_init)();
  hsa_status_t (*hsa_system_get_info)(hsa_system_info_t attribute, void* value);
};

HPCRUN_EXPOSED_API const struct hpcrun_foil_appdispatch_rocm_hsa
    hpcrun_dispatch_rocm_hsa;

#endif // HPCRUN_FOIL_ROCM_HSA_PRIVATE_H
