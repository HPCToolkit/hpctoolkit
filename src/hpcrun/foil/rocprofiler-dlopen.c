// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common.h"
#include "rocprofiler-private.h"

const struct hpcrun_foil_appdispatch_rocprofiler hpcrun_dispatch_rocprofiler = {
    .rocprofiler_open = &rocprofiler_open,
    .rocprofiler_close = &rocprofiler_close,
    .rocprofiler_get_metrics = &rocprofiler_get_metrics,
    .rocprofiler_set_queue_callbacks = &rocprofiler_set_queue_callbacks,
    .rocprofiler_start_queue_callbacks = &rocprofiler_start_queue_callbacks,
    .rocprofiler_stop_queue_callbacks = &rocprofiler_stop_queue_callbacks,
    .rocprofiler_remove_queue_callbacks = &rocprofiler_remove_queue_callbacks,
    .rocprofiler_iterate_info = &rocprofiler_iterate_info,
    .rocprofiler_group_get_data = &rocprofiler_group_get_data,
    .rocprofiler_get_group = &rocprofiler_get_group,
    .rocprofiler_error_string = &rocprofiler_error_string,
};

HPCRUN_EXPOSED_API void OnLoadToolProp(rocprofiler_settings_t* settings) {
  return hpcrun_foil_fetch_hooks_rocprofiler()->OnLoadToolProp(settings);
}

HPCRUN_EXPOSED_API void OnUnloadTool() {
  return hpcrun_foil_fetch_hooks_rocprofiler()->OnUnloadTool();
}
