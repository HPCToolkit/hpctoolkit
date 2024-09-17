// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCM_HSA_H
#define HPCRUN_FOIL_ROCM_HSA_H

#include <hsa/hsa.h>

hsa_status_t f_hsa_init();
hsa_status_t f_hsa_system_get_info(hsa_system_info_t attribute, void* value);

#endif // HPCRUN_FOIL_ROCM_HSA_H
