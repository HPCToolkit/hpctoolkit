// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../hpcrun-sonames.h"
#include "gtpin-private.h"

#include <dlfcn.h>

#include <mutex>

static const hpcrun_foil_appdispatch_gtpin* dispatch_var = nullptr;

static auto dispatch() noexcept -> const hpcrun_foil_appdispatch_gtpin* {
  static std::once_flag once;
  std::call_once(once, [] {
    void* handle =
        dlmopen(LM_ID_BASE, HPCRUN_DLOPEN_GTPIN_SO, RTLD_NOW | RTLD_DEEPBIND);
    if (handle == nullptr) {
      assert(false && "Failed to load " HPCRUN_DLOPEN_GTPIN_SO);
      abort();
    }
    dispatch_var =
        (const hpcrun_foil_appdispatch_gtpin*)dlsym(handle, "hpcrun_dispatch_gtpin");
    if (dispatch_var == nullptr) {
      assert(false && "Failed to fetch dispatch from " HPCRUN_DLOPEN_GTPIN_SO);
      abort();
    }
  });
  return dispatch_var;
}

auto gtpin::GTPin_GetCore() -> gtpin::IGtCore* { return dispatch()->GTPin_GetCore(); }
