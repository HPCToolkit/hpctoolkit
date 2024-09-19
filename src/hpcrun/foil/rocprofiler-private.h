// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCPROFILER_PRIVATE_H
#define HPCRUN_FOIL_ROCPROFILER_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "common.h"
#include "rocprofiler.h"

// These two are not defined anywhere in the rocprofiler headers, so we provide the
// definitions.
void OnLoadToolProp(rocprofiler_settings_t* settings);
void OnUnloadTool();

struct hpcrun_foil_appdispatch_rocprofiler {
  hsa_status_t (*rocprofiler_open)(hsa_agent_t agent, rocprofiler_feature_t* features,
                                   uint32_t feature_count, rocprofiler_t** context,
                                   uint32_t mode, rocprofiler_properties_t* properties);
  hsa_status_t (*rocprofiler_close)(rocprofiler_t* context);
  hsa_status_t (*rocprofiler_get_metrics)(const rocprofiler_t* context);
  hsa_status_t (*rocprofiler_set_queue_callbacks)(
      rocprofiler_queue_callbacks_t callbacks, void* data);
  hsa_status_t (*rocprofiler_start_queue_callbacks)();
  hsa_status_t (*rocprofiler_stop_queue_callbacks)();
  hsa_status_t (*rocprofiler_remove_queue_callbacks)();
  hsa_status_t (*rocprofiler_iterate_info)(
      const hsa_agent_t* agent, rocprofiler_info_kind_t kind,
      hsa_status_t (*callback)(const rocprofiler_info_data_t info, void* data),
      void* data);
  hsa_status_t (*rocprofiler_group_get_data)(rocprofiler_group_t* group);
  hsa_status_t (*rocprofiler_get_group)(rocprofiler_t* context, uint32_t index,
                                        rocprofiler_group_t* group);
  hsa_status_t (*rocprofiler_error_string)(const char** str);
};

HPCRUN_EXPOSED_API const struct hpcrun_foil_appdispatch_rocprofiler
    hpcrun_dispatch_rocprofiler;

struct hpcrun_foil_hookdispatch_rocprofiler {
  void (*OnLoadToolProp)(rocprofiler_settings_t* settings);
  void (*OnUnloadTool)();
};

#endif // HPCRUN_FOIL_ROCPROFILER_PRIVATE_H
