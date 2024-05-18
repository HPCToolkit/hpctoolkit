// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#define _GNU_SOURCE

#include "level0-fence-map.h"
#include "level0-handle-map.h"
#include "../../../../../common/lean/spinlock.h"

#include <stdio.h>

//******************************************************************************
// local variables
//******************************************************************************

static level0_handle_map_entry_t *fence_map_root = NULL;

static level0_handle_map_entry_t *fence_free_list = NULL;

static spinlock_t fence_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_fence_map_insert
(
  ze_fence_handle_t hFence,
  uint32_t numCommandLists,
  ze_command_list_handle_t* phCommandLists
)
{
  if (hFence == NULL) return;
  spinlock_lock(&fence_lock);
  uint64_t key = (uint64_t)hFence;

  // Create level0 fence data node
  level0_fence_data_t* data = (level0_fence_data_t*) malloc(sizeof(level0_fence_data_t));
  data->num = numCommandLists;
  data->list = (ze_command_list_handle_t*) malloc(sizeof(ze_command_list_handle_t) * numCommandLists);
  for (int i = 0; i < numCommandLists; ++i) {
    data->list[i] = phCommandLists[i];
  }

  // Insert the fence-data pair
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&fence_free_list, key, (level0_data_node_t*)data);
  level0_handle_map_insert(&fence_map_root, entry);

  spinlock_unlock(&fence_lock);
}

level0_fence_data_t*
level0_fence_map_lookup
(
  ze_fence_handle_t hFence
)
{
  spinlock_lock(&fence_lock);

  uint64_t key = (uint64_t)hFence;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&fence_map_root, key);
  level0_fence_data_t* result = NULL;
  if (entry != NULL) result =
    (level0_fence_data_t*) (*level0_handle_map_entry_data_get(entry));

  spinlock_unlock(&fence_lock);
  return result;
}

void
level0_fence_map_delete
(
  ze_fence_handle_t hFence
)
{
  spinlock_lock(&fence_lock);
  uint64_t key = (uint64_t)hFence;

  // First look up and free the data node
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&fence_map_root, key);

  // It is possible that the application will first
  // do a fence reset and then a fence destroy
  if (entry != NULL) {
    level0_fence_data_t* result =
        (level0_fence_data_t*) (*level0_handle_map_entry_data_get(entry));
    free(result->list);
    free(result);
  }

  // delete the entry
  level0_handle_map_delete(
    &fence_map_root,
    &fence_free_list,
    key
  );

  spinlock_unlock(&fence_lock);
}
