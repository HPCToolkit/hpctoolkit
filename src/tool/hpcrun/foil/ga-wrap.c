// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include "foil.h"
#include "sample-sources/ga-overrides.h"

extern global_array_t *GA;

FOIL_WRAP_DECL(pnga_create)
HPCRUN_EXPOSED logical
__wrap_pnga_create(Integer type, Integer ndim, Integer *dims, char* name,
                   Integer *chunk, Integer *g_a) {
  LOOKUP_FOIL_BASE(base, pnga_create);
  return base(&__real_pnga_create, GA, type, ndim, dims, name, chunk, g_a);
}

FOIL_WRAP_DECL(pnga_create_handle)
HPCRUN_EXPOSED Integer
__wrap_pnga_create_handle() {
  LOOKUP_FOIL_BASE(base, pnga_create_handle);
  return base(&__real_pnga_create_handle, GA);
}

FOIL_WRAP_DECL(pnga_get)
HPCRUN_EXPOSED void
__wrap_pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld) {
  LOOKUP_FOIL_BASE(base, pnga_get);
  return base(&__real_pnga_get, GA, g_a, lo, hi, buf, ld);
}

FOIL_WRAP_DECL(pnga_put)
HPCRUN_EXPOSED void
__wrap_pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld) {
  LOOKUP_FOIL_BASE(base, pnga_put);
  return base(&__real_pnga_put, GA, g_a, lo, hi, buf, ld);
}

FOIL_WRAP_DECL(pnga_acc)
HPCRUN_EXPOSED void
__wrap_pnga_acc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha) {
  LOOKUP_FOIL_BASE(base, pnga_acc);
  return base(&__real_pnga_acc, GA, g_a, lo, hi, buf, ld, alpha);
}

FOIL_WRAP_DECL(pnga_nbget)
HPCRUN_EXPOSED void
__wrap_pnga_nbget(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbget);
  return base(&__real_pnga_nbget, GA, g_a, lo, hi, buf, ld, nbhandle);
}

FOIL_WRAP_DECL(pnga_nbput)
HPCRUN_EXPOSED void
__wrap_pnga_nbput(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbput);
  return base(&__real_pnga_nbput, GA, g_a, lo, hi, buf, ld, nbhandle);
}

FOIL_WRAP_DECL(pnga_nbacc)
HPCRUN_EXPOSED void
__wrap_pnga_nbacc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbacc);
  return base(&__real_pnga_nbacc, GA, g_a, lo, hi, buf, ld, alpha, nbhandle);
}

FOIL_WRAP_DECL(pnga_nbwait)
HPCRUN_EXPOSED void
__wrap_pnga_nbwait(Integer *nbhandle) {
  LOOKUP_FOIL_BASE(base, pnga_nbwait);
  return base(&__real_pnga_nbwait, GA, nbhandle);
}

FOIL_WRAP_DECL(pnga_brdcst)
HPCRUN_EXPOSED void
__wrap_pnga_brdcst(Integer type, void *buf, Integer len, Integer originator) {
  LOOKUP_FOIL_BASE(base, pnga_brdcst);
  return base(&__real_pnga_brdcst, GA, type, buf, len, originator);
}

FOIL_WRAP_DECL(pnga_gop)
HPCRUN_EXPOSED void
__wrap_pnga_gop(Integer type, void *x, Integer n, char *op) {
  LOOKUP_FOIL_BASE(base, pnga_gop);
  return base(&__real_pnga_gop, GA, type, x, n, op);
}

FOIL_WRAP_DECL(pnga_sync)
HPCRUN_EXPOSED void
__wrap_pnga_sync() {
  LOOKUP_FOIL_BASE(base, pnga_sync);
  return base(&__real_pnga_sync, GA);
}
