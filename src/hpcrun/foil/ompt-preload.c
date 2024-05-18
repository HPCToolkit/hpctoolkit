// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../ompt/ompt-interface.h"
#include "../monitor-exts/openmp.h"

HPCRUN_EXPOSED ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version,
    const char *runtime_version) {
  LOOKUP_FOIL_BASE(base, ompt_start_tool);
  return base(omp_version, runtime_version);
}

HPCRUN_EXPOSED void* _mp_init() {
  LOOKUP_FOIL_BASE(base, _mp_init);
  FOIL_DLSYM(real, _mp_init);
  base();
  return real();
}
