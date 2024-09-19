// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "rocm-hsa-private.h"

const struct hpcrun_foil_appdispatch_rocm_hsa hpcrun_dispatch_rocm_hsa = {
    .hsa_init = &hsa_init,
    .hsa_system_get_info = &hsa_system_get_info,
};
