// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_GA_PRIVATE_H
#define HPCRUN_FOIL_GA_PRIVATE_H

#include "common.h"
#include "ga.h"

#ifdef __cplusplus
#error This is a C-only header
#endif

struct hpcrun_foil_appdispatch_ga {
  global_array_t* GA;
  logical (*pnga_create)(Integer type, Integer ndim, Integer* dims, char* name,
                         Integer* chunk, Integer* g_a);
  Integer (*pnga_create_handle)();
  void (*pnga_get)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld);
  void (*pnga_put)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld);
  void (*pnga_acc)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                   void* alpha);
  void (*pnga_nbget)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     Integer* nbhandle);
  void (*pnga_nbput)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     Integer* nbhandle);
  void (*pnga_nbacc)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     void* alpha, Integer* nbhandle);
  void (*pnga_nbwait)(Integer* nbhandle);
  void (*pnga_brdcst)(Integer type, void* buf, Integer len, Integer originator);
  void (*pnga_gop)(Integer type, void* x, Integer n, char* op);
  void (*pnga_sync)();
};

struct hpcrun_foil_hookdispatch_ga {
  logical (*pnga_create)(Integer type, Integer ndim, Integer* dims, char* name,
                         Integer* chunk, Integer* g_a,
                         const struct hpcrun_foil_appdispatch_ga*);
  Integer (*pnga_create_handle)(const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_get)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                   const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_put)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                   const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_acc)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                   void* alpha, const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_nbget)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_nbput)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_nbacc)(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                     void* alpha, Integer* nbhandle,
                     const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_nbwait)(Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_brdcst)(Integer type, void* buf, Integer len, Integer originator,
                      const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_gop)(Integer type, void* x, Integer n, char* op,
                   const struct hpcrun_foil_appdispatch_ga*);
  void (*pnga_sync)();
};

#endif // HPCRUN_FOIL_GA_PRIVATE_H
