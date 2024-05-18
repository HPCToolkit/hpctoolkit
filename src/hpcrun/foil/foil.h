// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef FOIL_FOIL_H
#define FOIL_FOIL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/// Macro marker for (foil) functions that need to be exposed symbols.
#define HPCRUN_EXPOSED __attribute__((visibility("default"))) extern

// The various function pointer caches use atpmics
static_assert(ATOMIC_POINTER_LOCK_FREE == 2, "Atomic pointers are not lock-free!");
typedef atomic_intptr_t hpcrun_pfn_cache_t;

/// Fetch the base function given the foil's symbol name. The return value is a
/// general pointer to the base function.
void* hpcrun_foil_base_lookup(const char* name);

/// Helper function to fetch a base function given a foil symbol name.
/// Always thread-safe and async-signal-safe.
static inline void* hpcrun_foil_base_lookup_cached(hpcrun_pfn_cache_t* cache, const char* name) {
  intptr_t result = atomic_load_explicit(cache, memory_order_acquire);
  if (result == 0 /* Not NULL, which (technically) may not be 0 */) {
    result = (intptr_t)hpcrun_foil_base_lookup(name);
    intptr_t expected = 0;
    if (!atomic_compare_exchange_strong(cache, &expected, result))
      assert(expected == result);
  }
  return (void*)result;
}

/// Helper macro to quickly get a base function with the right type signature in a foil.
#define LOOKUP_FOIL_BASE(VAR, NAME) \
  static hpcrun_pfn_cache_t _cache_foilbase_ ## VAR; \
  typeof(foilbase_ ## NAME)* VAR = hpcrun_foil_base_lookup_cached(&_cache_foilbase_ ## VAR, #NAME);

struct callback_data {
    const char* symbol;
    bool skip;
    void *skip_until_base;
    void *result;
};
static int phdr_callback(struct dl_phdr_info *info, size_t sz, void *data_v) {
  struct callback_data *data = data_v;

  // Decide whether to skip this object as being before libmonitor
  if (data->skip) {
    if (data->skip_until_base == (void*)info->dlpi_addr) {
      data->skip = false;
    }
    return 0;
  }

  // Call dlopen to stabilize a handle for our use
  void *this = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD | RTLD_LOCAL);
  if (this == NULL) {
    return 0;
  }

  // Poke it and see if we can find the symbol we want. Stop if we found it,
  // otherwise continue the scan.
  data->result = dlsym(this, data->symbol);
  dlclose(this);
  return data->result == NULL ? 0 : 1;
}

/// Helper function to fetch the "real" function pointer for a symbol.
/// Usable only for LD_PRELOAD foils.
static inline void* hpcrun_foil_dlsym_cached(hpcrun_pfn_cache_t* cache, const char* name) {
  intptr_t result = atomic_load_explicit(cache, memory_order_acquire);
  if (result == 0 /* Not NULL, which (technically) may not be 0 */) {
    dlerror();
    result = (intptr_t)dlsym(RTLD_NEXT, name);
    const char* err = dlerror();
    if (err != NULL) {
      // Fallback: try to find the symbol by inspecting every object loaded
      // after us. This is similar to but not identical to RTLD_NEXT, since we
      // also inspect objects that were loaded with dlopen(RTLD_LOCAL).

      struct callback_data cb_data = {
          .symbol = name,
          .skip = true,
          .skip_until_base = NULL,
          .result = NULL,
      };

      // Identify ourselves, using our base address
      Dl_info dli;
      if (dladdr(&hpcrun_foil_dlsym_cached, &dli) == 0) {
          // This should never happen, but if it does error
          assert(false && "dladdr1 failed to find libmonitor\n");
          abort();
      }
      cb_data.skip_until_base = dli.dli_fbase;

      // Use dl_iterate_phdr to scan through all the objects
      int found = dl_iterate_phdr(phdr_callback, &cb_data);
      if (found == 0 || cb_data.result == NULL) {
          // We didn't find the required symbol, this is a hard error.
          fprintf(stderr, "dlsym(%s) failed: %s\n", name, err);
          abort();
      }
      result = (intptr_t)cb_data.result;
    }

    intptr_t expected = 0;
    if (!atomic_compare_exchange_strong(cache, &expected, result))
      assert(expected == result);
  }
  return (void*)result;
}

/// Helper macro to quickly get a "real" function pointer with the right type signature.
/// Usable only for LD_PRELOAD foils.
#define FOIL_DLSYM(VAR, NAME) \
  static hpcrun_pfn_cache_t _cache_real_ ## VAR; \
  typeof(NAME)* VAR = hpcrun_foil_dlsym_cached(&_cache_real_ ## VAR, #NAME);

/// Helper macro to generate a function to lazily fetch a symbol with dlsym.
/// The name of this function will always be pfn_<symbol>.
/// Usable only for LD_PRELOAD foils.
#define FOIL_DLSYM_FUNC(SYMBOL) \
  static typeof(&SYMBOL) pfn_ ## SYMBOL() { FOIL_DLSYM(pfn, SYMBOL); return pfn; }

/// Variant of #hpcrun_foil_dlsym_cached that allows specifying the version (dlvsym).
/// Usable only for LD_PRELOAD foils.
static inline void* hpcrun_foil_dlvsym_cached(hpcrun_pfn_cache_t* cache, const char* name,
    const char* version) {
  intptr_t result = atomic_load_explicit(cache, memory_order_acquire);
  if (result == 0 /* Not NULL, which (technically) may not be 0 */) {
    result = (intptr_t)dlvsym(RTLD_NEXT, name, version);
    intptr_t expected = 0;
    if (!atomic_compare_exchange_strong(cache, &expected, result))
      assert(expected == result);
  }
  return (void*)result;
}

/// Variant of #FOIL_DLSYM that allows specifying the version (dlvsym).
/// Usable only for LD_PRELOAD foils.
#define FOIL_DLVSYM(VAR, NAME, VERSION) \
  static hpcrun_pfn_cache_t _cache_real_ ## VAR; \
  typeof(NAME)* VAR = hpcrun_foil_dlvsym_cached(&_cache_real_ ## VAR, #NAME, VERSION);

#endif  // FOIL_FOIL_H
