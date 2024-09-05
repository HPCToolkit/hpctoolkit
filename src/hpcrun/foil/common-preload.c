// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "common-preload.h"

#include <assert.h>
#include <dlfcn.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

struct callback_data {
  const char* symbol;
  bool skip;
  void* skip_until_base;
  void* result;
};

static int phdr_callback(struct dl_phdr_info* info, size_t sz, void* data_v) {
  struct callback_data* data = data_v;

  // Decide whether to skip this object as being before libmonitor
  if (data->skip) {
    if ((ElfW(Addr))data->skip_until_base == info->dlpi_addr) {
      data->skip = false;
    }
    return 0;
  }

  // Call dlopen to stabilize a handle for our use
  void* this = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD | RTLD_LOCAL);
  if (this == NULL) {
    return 0;
  }

  // Poke it and see if we can find the symbol we want. Stop if we found it,
  // otherwise continue the scan.
  data->result = dlsym(this, data->symbol);
  dlclose(this);
  return data->result == NULL ? 0 : 1;
}

void* foil_dlsym(const char* symbol) {
  dlerror();
  void* result = dlsym(RTLD_NEXT, symbol);
  const char* err = dlerror();
  if (err != NULL) {
    // Fallback: try to find the symbol by inspecting every object loaded
    // after us. This is similar to but not identical to RTLD_NEXT, since we
    // also inspect objects that were loaded with dlopen(RTLD_LOCAL).

    struct callback_data cb_data = {
        .symbol = symbol,
        .skip = true,
        .skip_until_base = NULL,
        .result = NULL,
    };

    // Identify ourselves, using our base address
    Dl_info dli;
    if (dladdr(&foil_dlsym, &dli) == 0) {
      // This should never happen, but if it does error
      assert(false && "dladdr1 failed to find libmonitor\n");
      abort();
    }
    cb_data.skip_until_base = dli.dli_fbase;

    // Use dl_iterate_phdr to scan through all the objects
    int found = dl_iterate_phdr(phdr_callback, &cb_data);
    if (found == 0 || cb_data.result == NULL) {
      return NULL;
    }
    result = cb_data.result;
  }
  return result;
}

void* foil_dlvsym(const char* restrict symbol, const char* restrict version) {
  dlerror();
  void* result = dlvsym(RTLD_NEXT, symbol, version);
  const char* err = dlerror();
  if (err != NULL) {
    // Fallback: try to find an unversioned symbol if the versioned symbol is unavailable.
    return foil_dlsym(symbol);
  }
  return result;
}

static once_flag load_core_foil_once = ONCE_FLAG_INIT;

static struct pfn_fetch_hooks_t {
  const struct hpcrun_foil_hookdispatch_client* (*client)();
  const struct hpcrun_foil_hookdispatch_libc* (*libc)();
  const struct hpcrun_foil_hookdispatch_ga* (*ga)();
  const struct hpcrun_foil_hookdispatch_mpi* (*mpi)();
  const struct hpcrun_foil_hookdispatch_ompt* (*ompt)();
  const struct hpcrun_foil_hookdispatch_opencl* (*opencl)();
  const struct hpcrun_foil_hookdispatch_level0* (*level0)();
} pfn_fetch_hooks;

static void* core_dlsym(void* handle, const char* symbol) {
  void* result = dlsym(handle, symbol);
  if (result == NULL) {
    fprintf(stderr, "hpcrun: WARNING: internal symbol %s not found: %s\n", symbol,
            dlerror());
  }
  return result;
}

static void load_core_foil() {
  static void* h = NULL;
  h = dlopen("libhpcrun.so", RTLD_NOW | RTLD_LOCAL);
  if (h == NULL) {
    fprintf(stderr, "hpcrun: Error loading libhpcrun.so: %s\n", dlerror());
    abort();
  }
  pfn_fetch_hooks = (struct pfn_fetch_hooks_t){
      .client = core_dlsym(h, "hpcrun_foil_fetch_hooks_client"),
      .libc = core_dlsym(h, "hpcrun_foil_fetch_hooks_libc"),
      .ga = core_dlsym(h, "hpcrun_foil_fetch_hooks_ga"),
      .mpi = core_dlsym(h, "hpcrun_foil_fetch_hooks_mpi"),
      .ompt = core_dlsym(h, "hpcrun_foil_fetch_hooks_ompt"),
      .opencl = core_dlsym(h, "hpcrun_foil_fetch_hooks_opencl"),
      .level0 = core_dlsym(h, "hpcrun_foil_fetch_hooks_level0"),
  };
}

const struct hpcrun_foil_hookdispatch_client* hpcrun_foil_fetch_hooks_client_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.client();
}

const struct hpcrun_foil_hookdispatch_libc* hpcrun_foil_fetch_hooks_libc_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.libc();
}

const struct hpcrun_foil_hookdispatch_ga* hpcrun_foil_fetch_hooks_ga_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.ga();
}

const struct hpcrun_foil_hookdispatch_mpi* hpcrun_foil_fetch_hooks_mpi_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.mpi();
}

const struct hpcrun_foil_hookdispatch_ompt* hpcrun_foil_fetch_hooks_ompt_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.ompt();
}

const struct hpcrun_foil_hookdispatch_opencl* hpcrun_foil_fetch_hooks_opencl_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.opencl();
}

const struct hpcrun_foil_hookdispatch_level0* hpcrun_foil_fetch_hooks_level0_dl() {
  call_once(&load_core_foil_once, load_core_foil);
  return pfn_fetch_hooks.level0();
}
