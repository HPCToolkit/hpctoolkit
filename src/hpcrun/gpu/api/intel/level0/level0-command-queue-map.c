// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#define _GNU_SOURCE

#include "level0-command-queue-map.h"
#include "level0-handle-map.h"
#include "../../../../../common/lean/spinlock.h"

#include <stdio.h>

//******************************************************************************
// local variables
//******************************************************************************

static level0_handle_map_entry_t *command_queue_map_root = NULL;

static level0_handle_map_entry_t *command_queue_free_list = NULL;

static spinlock_t command_queue_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_command_queue_map_insert
(
  ze_command_queue_handle_t hcommand_queue,
  uint32_t numCommandLists,
  ze_command_list_handle_t* phCommandLists
)
{
  if (hcommand_queue == NULL) return;
  level0_command_queue_data_t* existing = level0_command_queue_map_lookup(hcommand_queue);

  // Create level0 command_queue data node
  level0_command_queue_data_t* data = (level0_command_queue_data_t*) malloc(sizeof(level0_command_queue_data_t));
  data->num = numCommandLists;
  // We need to combine existing command list with the current one
  if (existing != NULL) data->num += existing->num;
  data->list = (ze_command_list_handle_t*) malloc(sizeof(ze_command_list_handle_t) * data->num);
  for (int i = 0; i < numCommandLists; ++i) {
    data->list[i] = phCommandLists[i];
  }
  if (existing != NULL) {
    for (int i = 0; i < existing->num; ++i) {
      data->list[i + numCommandLists] = existing->list[i];
    }
    level0_command_queue_map_delete(hcommand_queue);
  }

  spinlock_lock(&command_queue_lock);
  uint64_t key = (uint64_t)hcommand_queue;

  // Insert the command_queue-data pair
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&command_queue_free_list, key, (level0_data_node_t*)data);
  level0_handle_map_insert(&command_queue_map_root, entry);

  spinlock_unlock(&command_queue_lock);
}

level0_command_queue_data_t*
level0_command_queue_map_lookup
(
  ze_command_queue_handle_t hcommand_queue
)
{
  spinlock_lock(&command_queue_lock);

  uint64_t key = (uint64_t)hcommand_queue;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&command_queue_map_root, key);
  level0_command_queue_data_t* result = NULL;
  if (entry != NULL) result =
    (level0_command_queue_data_t*) (*level0_handle_map_entry_data_get(entry));

  spinlock_unlock(&command_queue_lock);
  return result;
}

void
level0_command_queue_map_delete
(
  ze_command_queue_handle_t hcommand_queue
)
{
  spinlock_lock(&command_queue_lock);
  uint64_t key = (uint64_t)hcommand_queue;

  // First look up and free the data node
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&command_queue_map_root, key);

  // It is possible that the application will first
  // do a command_queue reset and then a command_queue destroy
  if (entry != NULL) {
    level0_command_queue_data_t* result =
        (level0_command_queue_data_t*) (*level0_handle_map_entry_data_get(entry));
    free(result->list);
    free(result);
  }

  // delete the entry
  level0_handle_map_delete(
    &command_queue_map_root,
    &command_queue_free_list,
    key
  );

  spinlock_unlock(&command_queue_lock);
}
