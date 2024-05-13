// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __MEMLEAK_OVERRIDES_H__
#define __MEMLEAK_OVERRIDES_H__

#include <stddef.h>

typedef void *memalign_fcn(size_t, size_t);
typedef void *valloc_fcn(size_t);
typedef void *malloc_fcn(size_t);
typedef void  free_fcn(void *);
typedef void *realloc_fcn(void *, size_t);

extern int foilbase_posix_memalign(memalign_fcn*, malloc_fcn*, void **memptr, size_t alignment, size_t bytes);
extern void *foilbase_memalign(memalign_fcn*, malloc_fcn*, size_t boundary, size_t bytes);
extern void *foilbase_valloc(memalign_fcn*, malloc_fcn*, valloc_fcn*, size_t bytes);
extern void *foilbase_malloc(memalign_fcn*, malloc_fcn*, size_t bytes);
extern void *foilbase_calloc(memalign_fcn*, malloc_fcn*, size_t nmemb, size_t bytes);
extern void foilbase_free(free_fcn*, void *ptr);
extern void *foilbase_realloc(memalign_fcn*, malloc_fcn*, realloc_fcn*, free_fcn*, void *ptr, size_t bytes);

#endif
