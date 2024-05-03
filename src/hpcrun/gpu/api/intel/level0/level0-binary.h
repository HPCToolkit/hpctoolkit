// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_binary_h
#define level0_binary_h

//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../common/gpu-binary.h"

#include "level0-api.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
level0_binary_process
(
  ze_module_handle_t module
);


void
level0_module_handle_map_lookup
(
  ze_module_handle_t module,
  const char **hash_string,
  gpu_binary_kind_t *bkind
);


void
level0_module_handle_map_delete
(
  ze_module_handle_t module
);

#endif
