// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <string.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-handle-map.h"
#include "level0-event-map.h"
#include "../../../../../common/lean/spinlock.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../../../common/gpu-print.h"

//******************************************************************************
// local data
//******************************************************************************

static level0_handle_map_entry_t *event_map_root = NULL;

static level0_handle_map_entry_t *event_free_list = NULL;

static spinlock_t event_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t*
level0_event_map_lookup
(
 ze_event_handle_t event_handle
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_entry_t *result = level0_handle_map_lookup(&event_map_root, key);

  PRINT("level0 event map lookup: id=0x%lx (record %p)\n",
       key, result);

  spinlock_unlock(&event_map_lock);
  if (result == NULL) return NULL;
  return *level0_handle_map_entry_data_get(result);
}

void
level0_event_map_insert
(
 ze_event_handle_t event_handle,
 level0_data_node_t* new_data
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_entry_t *entry = level0_handle_map_entry_new(&event_free_list, key, new_data);

  level0_handle_map_insert(&event_map_root, entry);

  PRINT("level0 event map insert: handle=%p (entry=%p)\n",
         command_list_handle, new_entry);

  spinlock_unlock(&event_map_lock);
}

void
level0_event_map_delete
(
 ze_event_handle_t event_handle
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_delete(&event_map_root, &event_free_list, key);

  spinlock_unlock(&event_map_lock);
}
