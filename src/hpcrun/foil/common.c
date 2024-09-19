// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ENABLE_OPENCL
const struct hpcrun_foil_hookdispatch_opencl* hpcrun_foil_fetch_hooks_opencl() {
  fprintf(stderr,
          "hpcrun: Internal library not built with OpenCL support, cannot continue!\n");
  abort();
}
#endif

#ifndef USE_ROCM
const struct hpcrun_foil_hookdispatch_rocprofiler*
hpcrun_foil_fetch_hooks_rocprofiler() {
  fprintf(stderr,
          "hpcrun: Internal library not built with ROCm support, cannot continue!\n");
  abort();
}
#endif

#ifndef USE_LEVEL0
const struct hpcrun_foil_hookdispatch_level0* hpcrun_foil_fetch_hooks_level0() {
  fprintf(
      stderr,
      "hpcrun: Internal library not built with Level Zero support, cannot continue!\n");
  abort();
}
#endif
