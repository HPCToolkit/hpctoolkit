// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __MEMLEAK_OVERRIDES_H__
#define __MEMLEAK_OVERRIDES_H__

#include "../foil/libc.h"

extern int hpcrun_posix_memalign(
    void **memptr, size_t alignment, size_t bytes,
    const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void *hpcrun_memalign(size_t boundary, size_t bytes,
                               const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void *hpcrun_valloc(size_t bytes,
                             const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void *hpcrun_malloc_intercept(size_t bytes,
                             const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void *hpcrun_calloc(size_t nmemb, size_t bytes,
                             const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void hpcrun_free(void *ptr,
                          const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
extern void *hpcrun_realloc(void *ptr, size_t bytes,
                              const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);

#endif
