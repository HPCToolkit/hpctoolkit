// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "libc-private.h"

#include <assert.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

// These are internal Glibc names for the allocation functions. Not defined anywhere so
// we define them ourselves.
extern typeof(memalign) __libc_memalign; // NOLINT(bugprone-reserved-identifier)
extern typeof(valloc) __libc_valloc;     // NOLINT(bugprone-reserved-identifier)
extern typeof(malloc) __libc_malloc;     // NOLINT(bugprone-reserved-identifier)
extern typeof(calloc) __libc_calloc;     // NOLINT(bugprone-reserved-identifier)
extern typeof(free) __libc_free;         // NOLINT(bugprone-reserved-identifier)
extern typeof(realloc) __libc_realloc;   // NOLINT(bugprone-reserved-identifier)

static struct hpcrun_foil_appdispatch_libc_alloc dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_libc_alloc){
      .memalign = __libc_memalign,
      .valloc = __libc_valloc,
      .malloc = __libc_malloc,
      .calloc = __libc_calloc,
      .free = __libc_free,
      .realloc = __libc_realloc,
  };
}

static const struct hpcrun_foil_appdispatch_libc_alloc* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

// malloc and friends are called very early in the process. Because of problematic
// recursion we can't call dlopen (or much else) before some sort of "clean point." We
// use the init constructor of this library itself as that clean point, it's a little
// late but it works in most cases.
//
// We also make sure the hooks are fetched *before* the clean point, so that any
// recursive cases are naturally avoided.
static atomic_bool passthrough = true;
static const struct hpcrun_foil_hookdispatch_libc* hooks;

__attribute__((constructor)) static void clean_point() {
  hooks = hpcrun_foil_fetch_hooks_libc_dl();
  atomic_store_explicit(&passthrough, false, memory_order_release);
}

HPCRUN_EXPOSED_API int posix_memalign(void** memptr, size_t alignment, size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    abort();
  return hooks->posix_memalign(memptr, alignment, bytes, dispatch());
}

HPCRUN_EXPOSED_API void* memalign(size_t boundary, size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->memalign(boundary, bytes);
  return hooks->memalign(boundary, bytes, dispatch());
}

HPCRUN_EXPOSED_API void* valloc(size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->valloc(bytes);
  return hooks->valloc(bytes, dispatch());
}

HPCRUN_EXPOSED_API void* malloc(size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->malloc(bytes);
  return hooks->malloc(bytes, dispatch());
}

HPCRUN_EXPOSED_API void* calloc(size_t nmemb, size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->calloc(nmemb, bytes);
  return hooks->calloc(nmemb, bytes, dispatch());
}

HPCRUN_EXPOSED_API void free(void* ptr) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->free(ptr);
  return hooks->free(ptr, dispatch());
}

HPCRUN_EXPOSED_API void* realloc(void* ptr, size_t bytes) {
  if (atomic_load_explicit(&passthrough, memory_order_acquire))
    return dispatch()->realloc(ptr, bytes);
  return hooks->realloc(ptr, bytes, dispatch());
}
