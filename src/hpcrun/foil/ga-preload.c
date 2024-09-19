// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "ga-private.h"

#include <threads.h>

static struct hpcrun_foil_appdispatch_ga dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_ga){
      .GA = foil_dlsym("GA"),
      .pnga_create = foil_dlsym("pnga_create"),
      .pnga_create_handle = foil_dlsym("pnga_create_handle"),
      .pnga_get = foil_dlsym("pnga_get"),
      .pnga_put = foil_dlsym("pnga_put"),
      .pnga_acc = foil_dlsym("pnga_acc"),
      .pnga_nbget = foil_dlsym("pnga_nbget"),
      .pnga_nbput = foil_dlsym("pnga_nbput"),
      .pnga_nbacc = foil_dlsym("pnga_nbacc"),
      .pnga_nbwait = foil_dlsym("pnga_nbwait"),
      .pnga_brdcst = foil_dlsym("pnga_brdcst"),
      .pnga_gop = foil_dlsym("pnga_gop"),
      .pnga_sync = foil_dlsym("pnga_sync"),
  };
}

static const struct hpcrun_foil_appdispatch_ga* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API logical pnga_create(Integer type, Integer ndim, Integer* dims,
                                       char* name, Integer* chunk, Integer* g_a) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_create(type, ndim, dims, name, chunk,
                                                      g_a, dispatch());
}

HPCRUN_EXPOSED_API Integer pnga_create_handle() {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_create_handle(dispatch());
}

HPCRUN_EXPOSED_API void pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                 Integer* ld) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_get(g_a, lo, hi, buf, ld, dispatch());
}

HPCRUN_EXPOSED_API void pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                 Integer* ld) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_put(g_a, lo, hi, buf, ld, dispatch());
}

HPCRUN_EXPOSED_API void pnga_acc(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                 Integer* ld, void* alpha) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_acc(g_a, lo, hi, buf, ld, alpha,
                                                   dispatch());
}

HPCRUN_EXPOSED_API void pnga_nbget(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                   Integer* ld, Integer* nbhandle) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_nbget(g_a, lo, hi, buf, ld, nbhandle,
                                                     dispatch());
}

HPCRUN_EXPOSED_API void pnga_nbput(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                   Integer* ld, Integer* nbhandle) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_nbput(g_a, lo, hi, buf, ld, nbhandle,
                                                     dispatch());
}

HPCRUN_EXPOSED_API void pnga_nbacc(Integer g_a, Integer* lo, Integer* hi, void* buf,
                                   Integer* ld, void* alpha, Integer* nbhandle) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_nbacc(g_a, lo, hi, buf, ld, alpha,
                                                     nbhandle, dispatch());
}

HPCRUN_EXPOSED_API void pnga_nbwait(Integer* nbhandle) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_nbwait(nbhandle, dispatch());
}

HPCRUN_EXPOSED_API void pnga_brdcst(Integer type, void* buf, Integer len,
                                    Integer originator) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_brdcst(type, buf, len, originator,
                                                      dispatch());
}

HPCRUN_EXPOSED_API void pnga_gop(Integer type, void* x, Integer n, char* op) {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_gop(type, x, n, op, dispatch());
}

HPCRUN_EXPOSED_API void pnga_sync() {
  return hpcrun_foil_fetch_hooks_ga_dl()->pnga_sync(dispatch());
}
