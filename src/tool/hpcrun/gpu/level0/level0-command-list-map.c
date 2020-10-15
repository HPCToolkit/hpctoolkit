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
// Copyright ((c)) 2002-2020, Rice University
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
// system includes
//*****************************************************************************

#include <assert.h>
#include <string.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-command-list-map.h"
#include "lib/prof-lean/spinlock.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

//******************************************************************************
// local data
//******************************************************************************

// TODO:
// Replace mutual exclusion with more efficient
// data structures.

static level0_handle_map_entry_t *commandlist_map_root = NULL;

static level0_handle_map_entry_t *commandlist_free_list = NULL;

static spinlock_t commandlist_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// private operations
//*****************************************************************************

static void
link_node
(
 level0_data_node_t** list,
 level0_data_node_t* node
)
{
  node->next = *list;
  *list = node;
}

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t**
level0_commandlist_map_lookup
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&commandlist_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_entry_t *result = level0_handle_map_lookup(&commandlist_map_root, key);

  PRINT("level0 commandlist map lookup: id=0x%lx (record %p)\n",
       key, result);

  spinlock_unlock(&commandlist_map_lock);
  if (result == NULL) return NULL;
  return level0_handle_map_entry_data_get(result);
}

void
level0_commandlist_map_insert
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&commandlist_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_entry_t *entry = level0_handle_map_entry_new(&commandlist_free_list, key, NULL);
  level0_handle_map_insert(&commandlist_map_root, entry);

  PRINT("level0 commandlist map insert: handle=%p (entry=%p)\n",
	 command_list_handle, entry);

  spinlock_unlock(&commandlist_map_lock);
}

void
level0_commandlist_map_delete
(
 ze_command_list_handle_t command_list_handle
)
{
  level0_data_node_t* command_list_head = *level0_commandlist_map_lookup(command_list_handle);
  level0_data_list_free(command_list_head);

  spinlock_lock(&commandlist_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_handle_map_delete(&commandlist_map_root, &commandlist_free_list, key);

  spinlock_unlock(&commandlist_map_lock);
}

level0_data_node_t*
level0_commandlist_alloc_kernel
(
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
)
{
  level0_data_node_t* list_entry = level0_data_node_new();
  list_entry->type = LEVEL0_KERNEL;
  list_entry->details.kernel.kernel = kernel;
  list_entry->event = event;
  list_entry->event_pool = event_pool;
  list_entry->next = NULL;
  return list_entry;
}

level0_data_node_t*
level0_commandlist_alloc_memcpy
(
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
)
{
  level0_data_node_t* list_entry = level0_data_node_new();
  list_entry->type = LEVEL0_MEMCPY;
  list_entry->details.memcpy.src_type = src_type;
  list_entry->details.memcpy.dst_type = dst_type;
  list_entry->details.memcpy.copy_size = copy_size;
  list_entry->event = event;
  list_entry->event_pool = event_pool;
  list_entry->next = NULL;
  return list_entry;
}

level0_data_node_t*
level0_commandlist_append_kernel
(
 level0_data_node_t** command_list,
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
)
{
  level0_data_node_t* list_entry = level0_commandlist_alloc_kernel(kernel, event, event_pool);
  link_node(command_list, list_entry);
  return list_entry;
}

level0_data_node_t*
level0_commandlist_append_memcpy
(
 level0_data_node_t** command_list,
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
)
{
  level0_data_node_t* list_entry = level0_commandlist_alloc_memcpy(src_type, dst_type, copy_size, event, event_pool);
  link_node(command_list, list_entry);
  return list_entry;
}
