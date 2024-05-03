// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../gpu/api/amd/rocprofiler-api.h"

// NB: The argument is actually a rocprofiler_settings_t, but we can't include rocprofiler.h here
HPCRUN_EXPOSED void OnLoadToolProp(void* settings) {
  LOOKUP_FOIL_BASE(base, OnLoadToolProp);
  return base(settings);
}

HPCRUN_EXPOSED void OnUnloadTool() {
  LOOKUP_FOIL_BASE(base, OnUnloadTool);
  return base();
}
