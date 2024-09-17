// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCPROFILER_H
#define HPCRUN_FOIL_ROCPROFILER_H

#ifndef __cplusplus
// This variable needs to be predefined as a weak symbol to prevent conflicts when
// compiling as C.
__attribute__((weak)) const unsigned HSA_VEN_AMD_AQLPROFILE_LEGACY_PM4_PACKET_SIZE;
#endif

#include <rocprofiler/rocprofiler.h>

hsa_status_t f_rocprofiler_open(hsa_agent_t agent, rocprofiler_feature_t* features,
                                uint32_t feature_count, rocprofiler_t** context,
                                uint32_t mode, rocprofiler_properties_t* properties);
hsa_status_t f_rocprofiler_close(rocprofiler_t* context);
hsa_status_t f_rocprofiler_get_metrics(rocprofiler_t* context);
hsa_status_t f_rocprofiler_set_queue_callbacks(rocprofiler_queue_callbacks_t callbacks,
                                               void* data);
hsa_status_t f_rocprofiler_start_queue_callbacks();
hsa_status_t f_rocprofiler_stop_queue_callbacks();
hsa_status_t f_rocprofiler_remove_queue_callbacks();
hsa_status_t f_rocprofiler_iterate_info(
    const hsa_agent_t* agent, rocprofiler_info_kind_t kind,
    hsa_status_t (*callback)(const rocprofiler_info_data_t info, void* data),
    void* data);
hsa_status_t f_rocprofiler_group_get_data(rocprofiler_group_t* group);
hsa_status_t f_rocprofiler_get_group(rocprofiler_t* context, uint32_t index,
                                     rocprofiler_group_t* group);
hsa_status_t f_rocprofiler_error_string(const char** str);

#endif // HPCRUN_FOIL_ROCPROFILER_H
