// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "gpu-kernel-table.h"
#include "../../../logical/common.h"


static bool did_init = false;
static logical_metadata_store_t kernel_metadata_store;


void gpu_kernel_table_init() {
  if(!did_init) {
    did_init = true;
    hpcrun_logical_metadata_register(&kernel_metadata_store, "gpu-kernel");
  }
}

ip_normalized_t gpu_kernel_table_get(const char* kernel_name, enum logical_mangling mangling) {
  uint32_t fid = hpcrun_logical_metadata_fid(&kernel_metadata_store,
      kernel_name, mangling, NULL, 0);
  return hpcrun_logical_metadata_ipnorm(&kernel_metadata_store, fid, 0);
}
