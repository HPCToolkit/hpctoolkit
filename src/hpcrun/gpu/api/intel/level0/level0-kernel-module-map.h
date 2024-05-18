// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_kernel_module_map_h
#define level0_kernel_module_map_h

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_kernel_module_map_insert
(
  ze_kernel_handle_t kernel,
  ze_module_handle_t module
);

ze_module_handle_t
level0_kernel_module_map_lookup
(
  ze_kernel_handle_t kernel
);

void
level0_kernel_module_map_delete
(
  ze_kernel_handle_t kernel
);

#endif
