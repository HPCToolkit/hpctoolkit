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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef FOIL_FOIL_H
#define FOIL_FOIL_H

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <dlfcn.h>

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

/// Helper function to fetch the "real" function pointer for a symbol.
/// Usable only for LD_PRELOAD foils.
static inline void* hpcrun_foil_dlsym_cached(hpcrun_pfn_cache_t* cache, const char* name) {
  intptr_t result = atomic_load_explicit(cache, memory_order_acquire);
  if (result == 0 /* Not NULL, which (technically) may not be 0 */) {
    result = (intptr_t)dlsym(RTLD_NEXT, name);
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
