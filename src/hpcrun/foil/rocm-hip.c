// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "rocm-hip.h"

#include "rocm-hip-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

static const struct hpcrun_foil_appdispatch_rocm_hip* dispatch_var = NULL;

static void init_dispatch() {
  void* handle =
      dlmopen(LM_ID_BASE, "libhpcrun_dlopen_rocm.so", RTLD_NOW | RTLD_DEEPBIND);
  if (handle == NULL) {
    assert(false && "Failed to load foil_rocm.so");
    abort();
  }
  dispatch_var = dlsym(handle, "hpcrun_dispatch_rocm_hip");
  if (dispatch_var == NULL) {
    assert(false && "Failed to fetch dispatch from foil_rocm.so");
    abort();
  }
}

static const struct hpcrun_foil_appdispatch_rocm_hip* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return dispatch_var;
}

hipError_t f_hipDeviceSynchronize() { return dispatch()->hipDeviceSynchronize(); }

hipError_t f_hipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attrib, int dev) {
  return dispatch()->hipDeviceGetAttribute(pi, attrib, dev);
}

hipError_t f_hipCtxGetCurrent(hipCtx_t* ctx) {
  return dispatch()->hipCtxGetCurrent(ctx);
}

hipError_t f_hipStreamSynchronize(hipStream_t stream) {
  return dispatch()->hipStreamSynchronize(stream);
}
