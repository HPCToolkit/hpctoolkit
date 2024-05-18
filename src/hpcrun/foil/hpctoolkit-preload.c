// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../start-stop.h"

#include "../hpctoolkit.h"

HPCRUN_EXPOSED int hpctoolkit_sampling_is_active() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_is_active);
  return base();
}

HPCRUN_EXPOSED void hpctoolkit_sampling_start() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_start);
  return base();
}

HPCRUN_EXPOSED void hpctoolkit_sampling_stop() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_stop);
  return base();
}

// Fortran aliases

// FIXME: The Fortran functions really need a separate API with
// different names to handle arguments and return values.  But
// hpctoolkit_sampling_start() and _stop() are void->void, so they're
// a special case.

HPCRUN_EXPOSED void hpctoolkit_sampling_start_ () __attribute__ ((alias ("hpctoolkit_sampling_start")));
HPCRUN_EXPOSED void hpctoolkit_sampling_start__() __attribute__ ((alias ("hpctoolkit_sampling_start")));

HPCRUN_EXPOSED void hpctoolkit_sampling_stop_ () __attribute__ ((alias ("hpctoolkit_sampling_stop")));
HPCRUN_EXPOSED void hpctoolkit_sampling_stop__() __attribute__ ((alias ("hpctoolkit_sampling_stop")));
