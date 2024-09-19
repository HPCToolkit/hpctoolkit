// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "ompt-private.h"

HPCRUN_EXPOSED_API ompt_start_tool_result_t*
ompt_start_tool(unsigned int omp_version, const char* runtime_version) {
  return hpcrun_foil_fetch_hooks_ompt_dl()->ompt_start_tool(omp_version,
                                                            runtime_version);
}

// The PGI OpenMP compiler does some strange things with their thread
// stacks.  We use _mp_init() as our test for this and then adjust the
// unwind heuristics if found.

// NOLINTNEXTLINE(bugprone-reserved-identifier)
HPCRUN_EXPOSED_API void* _mp_init() {
  hpcrun_foil_fetch_hooks_ompt_dl()->mp_init();
  return ((void* (*)())foil_dlsym("_mp_init"))();
}
