// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#define _GNU_SOURCE

#include "level0-handle-map.h"
#include "level0-kernel-module-map.h"
#include "../../../../../common/lean/spinlock.h"

#include <stdio.h>

//******************************************************************************
// local variables
//******************************************************************************

static level0_handle_map_entry_t *kernel_module_map_root = NULL;

static level0_handle_map_entry_t *kernel_module_free_list = NULL;

static spinlock_t kernel_module_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_kernel_module_map_insert
(
  ze_kernel_handle_t kernel,
  ze_module_handle_t module
)
{
  spinlock_lock(&kernel_module_lock);
  uint64_t key = (uint64_t)kernel;
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&kernel_module_free_list, key, (level0_data_node_t*)module);
  level0_handle_map_insert(&kernel_module_map_root, entry);

  spinlock_unlock(&kernel_module_lock);
}

ze_module_handle_t
level0_kernel_module_map_lookup
(
  ze_kernel_handle_t kernel
)
{
  spinlock_lock(&kernel_module_lock);

  uint64_t key = (uint64_t)kernel;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&kernel_module_map_root, key);
  ze_module_handle_t result =
    (ze_module_handle_t) (*level0_handle_map_entry_data_get(entry));

  spinlock_unlock(&kernel_module_lock);
  return result;
}

void
level0_kernel_module_map_delete
(
  ze_kernel_handle_t kernel
)
{
  spinlock_lock(&kernel_module_lock);
  uint64_t key = (uint64_t)kernel;
  level0_handle_map_delete(
    &kernel_module_map_root,
    &kernel_module_free_list,
    key
  );

  spinlock_unlock(&kernel_module_lock);
}
