// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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
