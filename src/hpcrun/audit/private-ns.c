// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#define _GNU_SOURCE

#include "binding.h"

#include <assert.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <link.h>

__attribute__((visibility("default")))
void hpcrun_bind_v(const char* libname, va_list bindings) {
  // Before anything else, try to load the library.
  void* handle = dlopen(libname, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    fprintf(stderr, "ERROR: Unable to bind to '%s': failed to load: %s\n", libname, dlerror());
    abort();
  }

  // Bind each requested symbol in turn
  dlerror();  // Clear any errors
  for (const char* sym = va_arg(bindings, const char*); sym != NULL;
       sym = va_arg(bindings, const char*)) {
    void** dst = va_arg(bindings, void**);
    *dst = dlsym(handle, sym);
    const char* error = dlerror();
    if (*dst == NULL && error != NULL) {
      fprintf(stderr, "ERROR: Unable to bind to '%s': %s\n", libname, error);
      abort();
    }
  }
}

// libunwind calls dl_iterate_phdr to acquire information about the load map, but dl_iterate_phdr
// is not async-signal-safe and will deadlock when called from a signal handler.
//
// The function and pointer below allow reimplementing the function with a custom version.
// The pointer must be filled with a suitable override before any calls to the function occur.

typedef int (*pfn_iterate_phdr_t)(int (*callback)(struct dl_phdr_info*, size_t, void*), void* data);
__attribute__((visibility("default")))
pfn_iterate_phdr_t hpcrun_iterate_phdr = NULL;
__attribute__((visibility("default")))
int dl_iterate_phdr(int (*callback)(struct dl_phdr_info*, size_t, void*), void* data) {
  if (hpcrun_iterate_phdr == NULL) {
    assert(false && "dl_iterate_phdr called but hpcrun_iterate_phdr override pointer not yet set!");
    abort();
  }
  return hpcrun_iterate_phdr(callback, data);
}
