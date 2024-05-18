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

#include "level0-command-list-context-map.h"
#include "../../../../../common/lean/spinlock.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../../../common/gpu-print.h"

//******************************************************************************
// local data
//******************************************************************************

// TODO:
// Replace mutual exclusion with more efficient
// data structures.

static level0_handle_map_entry_t *commandlist_context_map_root = NULL;

static level0_handle_map_entry_t *commandlist_context_free_list = NULL;

static spinlock_t commandlist_context_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// interface operations
//*****************************************************************************

ze_context_handle_t
level0_commandlist_context_map_lookup
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&commandlist_context_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&commandlist_context_map_root, key);
  ze_context_handle_t result =
    (ze_context_handle_t)(*level0_handle_map_entry_data_get(entry));

  PRINT("level0 commandlist context map lookup: id=0x%lx (context_handle %p)\n",
       key, result);

  spinlock_unlock(&commandlist_context_map_lock);
  return result;
}

void
level0_commandlist_context_map_insert
(
 ze_command_list_handle_t command_list_handle,
 ze_context_handle_t hContext
)
{
  spinlock_lock(&commandlist_context_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&commandlist_context_free_list, key, (level0_data_node_t*)hContext);
  level0_handle_map_insert(&commandlist_context_map_root, entry);

  PRINT("level0 commandlist map insert: handle=%p (entry=%p)\n",
         command_list_handle, entry);

  spinlock_unlock(&commandlist_context_map_lock);
}

void
level0_commandlist_context_map_delete
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&commandlist_context_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_delete(&commandlist_context_map_root, &commandlist_context_free_list, key);

  spinlock_unlock(&commandlist_context_map_lock);
}
