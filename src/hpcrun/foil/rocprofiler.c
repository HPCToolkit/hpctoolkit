// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "rocprofiler.h"

#include "../gpu/api/amd/rocprofiler-api.h"
#include "../hpcrun-sonames.h"
#include "common.h"
#include "rocprofiler-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

// Needed to cast the pointer to a void* for older code
static void OnLoadToolProp_wrap(rocprofiler_settings_t* settings) {
  return hpcrun_OnLoadToolProp(settings);
}

const struct hpcrun_foil_hookdispatch_rocprofiler*
hpcrun_foil_fetch_hooks_rocprofiler() {
  static const struct hpcrun_foil_hookdispatch_rocprofiler hooks = {
      .OnLoadToolProp = OnLoadToolProp_wrap,
      .OnUnloadTool = hpcrun_OnUnloadTool,
  };
  return &hooks;
}

static const struct hpcrun_foil_appdispatch_rocprofiler* dispatch_var = NULL;

static void init_dispatch() {
  void* handle = dlmopen(LM_ID_BASE, HPCRUN_DLOPEN_ROCM_SO, RTLD_NOW | RTLD_DEEPBIND);
  if (handle == NULL) {
    assert(false && "Failed to load foil_rocm.so");
    abort();
  }
  dispatch_var = dlsym(handle, "hpcrun_dispatch_rocprofiler");
  if (dispatch_var == NULL) {
    assert(false && "Failed to fetch dispatch from foil_rocm.so");
    abort();
  }
}

static const struct hpcrun_foil_appdispatch_rocprofiler* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return dispatch_var;
}

hsa_status_t f_rocprofiler_open(hsa_agent_t agent, rocprofiler_feature_t* features,
                                uint32_t feature_count, rocprofiler_t** context,
                                uint32_t mode, rocprofiler_properties_t* properties) {
  return dispatch()->rocprofiler_open(agent, features, feature_count, context, mode,
                                      properties);
}

hsa_status_t f_rocprofiler_close(rocprofiler_t* context) {
  return dispatch()->rocprofiler_close(context);
}

hsa_status_t f_rocprofiler_get_metrics(rocprofiler_t* context) {
  return dispatch()->rocprofiler_get_metrics(context);
}

hsa_status_t f_rocprofiler_set_queue_callbacks(rocprofiler_queue_callbacks_t callbacks,
                                               void* data) {
  return dispatch()->rocprofiler_set_queue_callbacks(callbacks, data);
}

hsa_status_t f_rocprofiler_start_queue_callbacks() {
  return dispatch()->rocprofiler_start_queue_callbacks();
}

hsa_status_t f_rocprofiler_stop_queue_callbacks() {
  return dispatch()->rocprofiler_stop_queue_callbacks();
}

hsa_status_t f_rocprofiler_remove_queue_callbacks() {
  return dispatch()->rocprofiler_remove_queue_callbacks();
}

hsa_status_t f_rocprofiler_iterate_info(
    const hsa_agent_t* agent, rocprofiler_info_kind_t kind,
    hsa_status_t (*callback)(const rocprofiler_info_data_t info, void* data),
    void* data) {
  return dispatch()->rocprofiler_iterate_info(agent, kind, callback, data);
}

hsa_status_t f_rocprofiler_group_get_data(rocprofiler_group_t* group) {
  return dispatch()->rocprofiler_group_get_data(group);
}

hsa_status_t f_rocprofiler_get_group(rocprofiler_t* context, uint32_t index,
                                     rocprofiler_group_t* group) {
  return dispatch()->rocprofiler_get_group(context, index, group);
}

hsa_status_t f_rocprofiler_error_string(const char** str) {
  return dispatch()->rocprofiler_error_string(str);
}
