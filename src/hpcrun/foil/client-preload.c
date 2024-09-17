// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../hpctoolkit.h"
#include "client-private.h"
#include "common-preload.h"
#include "common.h"

HPCRUN_EXPOSED_API int hpctoolkit_sampling_is_active() {
  return hpcrun_foil_fetch_hooks_client_dl()->sampling_is_active();
}

HPCRUN_EXPOSED_API void hpctoolkit_sampling_start() {
  return hpcrun_foil_fetch_hooks_client_dl()->sampling_start();
}

HPCRUN_EXPOSED_API void hpctoolkit_sampling_stop() {
  return hpcrun_foil_fetch_hooks_client_dl()->sampling_stop();
}

// Fortran aliases

// FIXME: The Fortran functions really need a separate API with
// different names to handle arguments and return values.  But
// hpctoolkit_sampling_start() and _stop() are void->void, so they're
// a special case.

HPCRUN_EXPOSED_API void hpctoolkit_sampling_start_()
    __attribute__((alias("hpctoolkit_sampling_start")));
HPCRUN_EXPOSED_API void hpctoolkit_sampling_start__()
    __attribute__((alias("hpctoolkit_sampling_start")));

HPCRUN_EXPOSED_API void hpctoolkit_sampling_stop_()
    __attribute__((alias("hpctoolkit_sampling_stop")));
HPCRUN_EXPOSED_API void hpctoolkit_sampling_stop__()
    __attribute__((alias("hpctoolkit_sampling_stop")));
