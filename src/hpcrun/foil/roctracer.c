// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "roctracer.h"

#include "roctracer-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

static const struct hpcrun_foil_appdispatch_roctracer* dispatch_var = NULL;

static void init_dispatch() {
  void* handle =
      dlmopen(LM_ID_BASE, "libhpcrun_dlopen_rocm.so", RTLD_NOW | RTLD_DEEPBIND);
  if (handle == NULL) {
    assert(false && "Failed to load foil_rocm.so");
    abort();
  }
  dispatch_var = dlsym(handle, "hpcrun_dispatch_roctracer");
  if (dispatch_var == NULL) {
    assert(false && "Failed to fetch dispatch from foil_rocm.so");
    abort();
  }
}

static const struct hpcrun_foil_appdispatch_roctracer* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return dispatch_var;
}

roctracer_status_t f_roctracer_open_pool_expl(const roctracer_properties_t* props,
                                              roctracer_pool_t** pool) {
  return dispatch()->roctracer_open_pool_expl(props, pool);
}

roctracer_status_t f_roctracer_flush_activity_expl(roctracer_pool_t* pool) {
  return dispatch()->roctracer_flush_activity_expl(pool);
}

roctracer_status_t
f_roctracer_activity_push_external_correlation_id(activity_correlation_id_t corr) {
  return dispatch()->roctracer_activity_push_external_correlation_id(corr);
}

roctracer_status_t
f_roctracer_activity_pop_external_correlation_id(activity_correlation_id_t* corr) {
  return dispatch()->roctracer_activity_pop_external_correlation_id(corr);
}

roctracer_status_t f_roctracer_enable_domain_callback(activity_domain_t dom,
                                                      activity_rtapi_callback_t cb,
                                                      void* ud) {
  return dispatch()->roctracer_enable_domain_callback(dom, cb, ud);
}

roctracer_status_t f_roctracer_enable_domain_activity_expl(activity_domain_t dom,
                                                           roctracer_pool_t* pool) {
  return dispatch()->roctracer_enable_domain_activity_expl(dom, pool);
}

roctracer_status_t f_roctracer_disable_domain_callback(activity_domain_t dom) {
  return dispatch()->roctracer_disable_domain_callback(dom);
}

roctracer_status_t f_roctracer_disable_domain_activity(activity_domain_t dom) {
  return dispatch()->roctracer_disable_domain_activity(dom);
}

roctracer_status_t f_roctracer_set_properties(activity_domain_t dom, void* props) {
  return dispatch()->roctracer_set_properties(dom, props);
}

const char* f_roctracer_op_string(uint32_t domain, uint32_t op, uint32_t kind) {
  return dispatch()->roctracer_op_string(domain, op, kind);
}
