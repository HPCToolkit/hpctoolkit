// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "ga.h"

#include "../sample-sources/ga-overrides.h"
#include "common.h"
#include "ga-private.h"

const struct hpcrun_foil_hookdispatch_ga* hpcrun_foil_fetch_hooks_ga() {
  static const struct hpcrun_foil_hookdispatch_ga hooks = {
      .pnga_create = hpcrun_pnga_create,
      .pnga_create_handle = hpcrun_pnga_create_handle,
      .pnga_get = hpcrun_pnga_get,
      .pnga_put = hpcrun_pnga_put,
      .pnga_acc = hpcrun_pnga_acc,
      .pnga_nbget = hpcrun_pnga_nbget,
      .pnga_nbput = hpcrun_pnga_nbput,
      .pnga_nbacc = hpcrun_pnga_nbacc,
      .pnga_nbwait = hpcrun_pnga_nbwait,
      .pnga_brdcst = hpcrun_pnga_brdcst,
      .pnga_gop = hpcrun_pnga_gop,
      .pnga_sync = hpcrun_pnga_sync,
  };
  return &hooks;
}

global_array_t* f_GA(const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->GA;
}

logical f_pnga_create(Integer type, Integer ndim, Integer* dims, char* name,
                      Integer* chunk, Integer* g_a,
                      const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_create(type, ndim, dims, name, chunk, g_a);
}

Integer f_pnga_create_handle(const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_create_handle();
}

void f_pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_get(g_a, lo, hi, buf, ld);
}

void f_pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_put(g_a, lo, hi, buf, ld);
}

void f_pnga_acc(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                void* alpha, const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_acc(g_a, lo, hi, buf, ld, alpha);
}

void f_pnga_nbget(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  Integer* nbhandle,
                  const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_nbget(g_a, lo, hi, buf, ld, nbhandle);
}

void f_pnga_nbput(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  Integer* nbhandle,
                  const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_nbput(g_a, lo, hi, buf, ld, nbhandle);
}

void f_pnga_nbacc(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  void* alpha, Integer* nbhandle,
                  const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_nbacc(g_a, lo, hi, buf, ld, alpha, nbhandle);
}

void f_pnga_nbwait(Integer* nbhandle,
                   const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_nbwait(nbhandle);
}

void f_pnga_brdcst(Integer type, void* buf, Integer len, Integer originator,
                   const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_brdcst(type, buf, len, originator);
}

void f_pnga_gop(Integer type, void* x, Integer n, char* op,
                const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_gop(type, x, n, op);
}

void f_pnga_sync(const struct hpcrun_foil_appdispatch_ga* dispatch) {
  return dispatch->pnga_sync();
}
