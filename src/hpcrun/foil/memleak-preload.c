// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../sample-sources/memleak-overrides.h"
#include <unistd.h>
#include <malloc.h>

extern typeof(memalign) __libc_memalign;
extern typeof(valloc) __libc_valloc;
extern typeof(malloc) __libc_malloc;
extern typeof(free) __libc_free;
extern typeof(realloc) __libc_realloc;

HPCRUN_EXPOSED int posix_memalign(void **memptr, size_t alignment, size_t bytes) {
  LOOKUP_FOIL_BASE(base, posix_memalign);
  return base(__libc_memalign, __libc_malloc, memptr, alignment, bytes);
}

HPCRUN_EXPOSED void *memalign(size_t boundary, size_t bytes) {
  LOOKUP_FOIL_BASE(base, memalign);
  return base(__libc_memalign, __libc_malloc, boundary, bytes);
}

HPCRUN_EXPOSED void *valloc(size_t bytes) {
  LOOKUP_FOIL_BASE(base, valloc);
  return base(__libc_memalign, __libc_malloc, __libc_valloc, bytes);
}

HPCRUN_EXPOSED void *malloc(size_t bytes) {
  LOOKUP_FOIL_BASE(base, malloc);
  return base(__libc_memalign, __libc_malloc, bytes);
}

HPCRUN_EXPOSED void *calloc(size_t nmemb, size_t bytes) {
  LOOKUP_FOIL_BASE(base, calloc);
  return base(__libc_memalign, __libc_malloc, nmemb, bytes);
}

HPCRUN_EXPOSED void free(void *ptr) {
  LOOKUP_FOIL_BASE(base, free);
  return base(__libc_free, ptr);
}

HPCRUN_EXPOSED void *realloc(void *ptr, size_t bytes) {
  LOOKUP_FOIL_BASE(base, realloc);
  return base(__libc_memalign, __libc_malloc, __libc_realloc, __libc_free, ptr, bytes);
}
