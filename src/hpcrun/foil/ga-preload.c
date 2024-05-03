// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../sample-sources/ga-overrides.h"

extern global_array_t *GA;

HPCRUN_EXPOSED logical
pnga_create(Integer type, Integer ndim, Integer *dims, char* name,
            Integer *chunk, Integer *g_a) {
  LOOKUP_FOIL_BASE(base, pnga_create);
  FOIL_DLSYM(real, pnga_create);
  return base(real, GA, type, ndim, dims, name, chunk, g_a);
}

HPCRUN_EXPOSED Integer
pnga_create_handle() {
  LOOKUP_FOIL_BASE(base, pnga_create_handle);
  FOIL_DLSYM(real, pnga_create_handle);
  return base(real, GA);
}

HPCRUN_EXPOSED void
pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld) {
  LOOKUP_FOIL_BASE(base, pnga_get);
  FOIL_DLSYM(real, pnga_get);
  return base(real, GA, g_a, lo, hi, buf, ld);
}

HPCRUN_EXPOSED void
pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld) {
  LOOKUP_FOIL_BASE(base, pnga_put);
  FOIL_DLSYM(real, pnga_put);
  return base(real, GA, g_a, lo, hi, buf, ld);
}

HPCRUN_EXPOSED void
pnga_acc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha) {
  LOOKUP_FOIL_BASE(base, pnga_acc);
  FOIL_DLSYM(real, pnga_acc);
  return base(real, GA, g_a, lo, hi, buf, ld, alpha);
}

HPCRUN_EXPOSED void
pnga_nbget(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbget);
  FOIL_DLSYM(real, pnga_nbget);
  return base(real, GA, g_a, lo, hi, buf, ld, nbhandle);
}

HPCRUN_EXPOSED void
pnga_nbput(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbput);
  FOIL_DLSYM(real, pnga_nbput);
  return base(real, GA, g_a, lo, hi, buf, ld, nbhandle);
}

HPCRUN_EXPOSED void
pnga_nbacc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbacc);
  FOIL_DLSYM(real, pnga_nbacc);
  return base(real, GA, g_a, lo, hi, buf, ld, alpha, nbhandle);
}

HPCRUN_EXPOSED void
pnga_nbwait(Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbwait);
  FOIL_DLSYM(real, pnga_nbwait);
  return base(real, GA, nbhandle);
}

HPCRUN_EXPOSED void
pnga_brdcst(Integer type, void *buf, Integer len, Integer originator) {
  LOOKUP_FOIL_BASE(base, pnga_brdcst);
  FOIL_DLSYM(real, pnga_brdcst);
  return base(real, GA, type, buf, len, originator);
}

HPCRUN_EXPOSED void
pnga_gop(Integer type, void *x, Integer n, char *op) {
  LOOKUP_FOIL_BASE(base, pnga_gop);
  FOIL_DLSYM(real, pnga_gop);
  return base(real, GA, type, x, n, op);
}

HPCRUN_EXPOSED void
pnga_sync() {
  LOOKUP_FOIL_BASE(base, pnga_sync);
  FOIL_DLSYM(real, pnga_sync);
  return base(real, GA);
}
