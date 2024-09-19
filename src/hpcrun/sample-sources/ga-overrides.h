// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HPCRUN_SS_GA_OVERRIDES_H
#define HPCRUN_SS_GA_OVERRIDES_H

#include "../foil/ga.h"

logical
hpcrun_pnga_create(
    Integer type, Integer ndim, Integer *dims, char* name,
    Integer *chunk, Integer *g_a, const struct hpcrun_foil_appdispatch_ga*);

Integer
hpcrun_pnga_create_handle(const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_acc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                void *alpha, const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_nbget(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_nbput(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_nbacc(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld,
                  void *alpha, Integer *nbhandle,
                  const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_nbwait(Integer *nbhandle, const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_brdcst(Integer type, void *buf, Integer len, Integer originator,
                   const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_gop(Integer type, void *x, Integer n, char *op,
                const struct hpcrun_foil_appdispatch_ga*);

void
hpcrun_pnga_sync(const struct hpcrun_foil_appdispatch_ga*);

// TODO: ga_pgroup_sync
void hpcrun_pgroup_sync(Integer grp_id, const struct hpcrun_foil_appdispatch_ga*);

// TODO: ga_pgroup_dgop
void hpcrun_pgroup_gop(Integer p_grp, Integer type, void *x, Integer n, char *op,
                       const struct hpcrun_foil_appdispatch_ga*);

#endif  // HPCRUN_SS_GA_OVERRIDES_H
