// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "rocm-hsa.h"

#include "../hpcrun-sonames.h"
#include "rocm-hsa-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

static const struct hpcrun_foil_appdispatch_rocm_hsa* dispatch_var = NULL;

static void init_dispatch() {
  void* handle = dlmopen(LM_ID_BASE, HPCRUN_DLOPEN_ROCM_SO, RTLD_NOW | RTLD_DEEPBIND);
  if (handle == NULL) {
    assert(false && "Failed to load foil_rocm.so");
    abort();
  }
  dispatch_var = dlsym(handle, "hpcrun_dispatch_rocm_hsa");
  if (dispatch_var == NULL) {
    assert(false && "Failed to fetch dispatch from foil_rocm.so");
    abort();
  }
}

static const struct hpcrun_foil_appdispatch_rocm_hsa* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return dispatch_var;
}

hsa_status_t f_hsa_init() { return dispatch()->hsa_init(); }

hsa_status_t f_hsa_system_get_info(hsa_system_info_t attribute, void* value) {
  return dispatch()->hsa_system_get_info(attribute, value);
}
