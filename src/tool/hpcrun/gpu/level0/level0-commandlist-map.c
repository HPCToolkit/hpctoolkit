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

#include "level0-commandlist-map.h"

#include "lib/prof-lean/splay-uint64.h"
#include "lib/prof-lean/spinlock.h"
#include "hpcrun/memory/hpcrun-malloc.h"
#include "hpcrun/gpu/gpu-splay-allocator.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"


#define st_insert				\
  typed_splay_insert(commandlist_handle)

#define st_lookup				\
  typed_splay_lookup(commandlist_handle)

#define st_delete				\
  typed_splay_delete(commandlist_handle)

#define st_forall				\
  typed_splay_forall(commandlist_handle)

#define st_count				\
  typed_splay_count(commandlist_handle)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, level0_commandlist_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(commandlist_handle) level0_commandlist_map_entry_t

typedef struct typed_splay_node(commandlist_handle) {
  struct typed_splay_node(commandlist_handle) *left;
  struct typed_splay_node(commandlist_handle) *right;
  uint64_t level0_commandlist_handle; // key
  level0_kernel_list_entry_t* head;
} typed_splay_node(commandlist_handle); 


//******************************************************************************
// local data
//******************************************************************************

static level0_commandlist_map_entry_t *map_root = NULL;

static level0_commandlist_map_entry_t *free_list = NULL;

static spinlock_t command_list_map_lock = SPINLOCK_UNLOCKED;

static level0_kernel_list_entry_t * kernel_list_entry_free_list = NULL;

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(commandlist_handle)


static level0_commandlist_map_entry_t *
level0_commandlist_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static level0_commandlist_map_entry_t *
level0_commandlist_id_map_entry_new
(
 ze_command_list_handle_t command_list_handle  
)
{
  level0_commandlist_map_entry_t *e = level0_commandlist_map_entry_alloc();

  memset(e, 0, sizeof(level0_commandlist_map_entry_t)); 

  e->level0_commandlist_handle = (uint64_t)command_list_handle;
  e->head = NULL;

  return e;
}

// Allocate a node for the linked list representing a command list
static level0_kernel_list_entry_t*
level0_commandlist_map_kernel_list_node_new
(
)
{
  level0_kernel_list_entry_t *first = kernel_list_entry_free_list; 

  if (first) { 
    kernel_list_entry_free_list = first->next;
  } else {
    first = (level0_kernel_list_entry_t *) hpcrun_malloc_safe(sizeof(level0_kernel_list_entry_t));
  }

  memset(first, 0, sizeof(level0_kernel_list_entry_t)); 

  return first;  
}

// Return a node for the linked list to the free list
static void
level0_commandlist_map_kernel_list_node_free
(
  level0_kernel_list_entry_t* node
)
{
  node->next = kernel_list_entry_free_list;
  kernel_list_entry_free_list = node;
}


static void
level0_commandlist_map_kernel_list_free
(
 level0_kernel_list_entry_t* head
)
{
  level0_kernel_list_entry_t* next;
  for (; head != NULL; head=next) {
    next = head->next;
    level0_commandlist_map_kernel_list_node_free(head);
  }
}


//*****************************************************************************
// interface operations
//*****************************************************************************

level0_commandlist_map_entry_t *
level0_commandlist_map_lookup
(
 ze_command_list_handle_t command_list_handle
)
{  
  spinlock_lock(&command_list_map_lock);

  uint64_t key = (uint64_t)command_list_handle;
  level0_commandlist_map_entry_t *result = st_lookup(&map_root, key);

  PRINT("commandlist map lookup: id=0x%lx (record %p)\n", 
       key, result); 

  spinlock_unlock(&command_list_map_lock);
  return result;
}

void
level0_commandlist_map_insert
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&command_list_map_lock);

  level0_commandlist_map_entry_t *entry = 
    level0_commandlist_id_map_entry_new(command_list_handle);

  st_insert(&map_root, entry);

  PRINT("level0_commandlist map insert: handle=%p (entry=%p)\n", 
	 command_list_handle, entry);

  spinlock_unlock(&command_list_map_lock);
}

void
level0_commandlist_map_delete
(
 ze_command_list_handle_t command_list_handle
)
{
  spinlock_lock(&command_list_map_lock);

  level0_commandlist_map_entry_t *node = st_delete(&map_root, (uint64_t)command_list_handle);
  level0_commandlist_map_kernel_list_free(node->head);
  st_free(&free_list, node);

  spinlock_unlock(&command_list_map_lock);
}

level0_kernel_list_entry_t*
level0_commandlist_map_kernel_list_get
(
 level0_commandlist_map_entry_t *entry
)
{
  return entry->head;
}

void
level0_commandlist_map_kernel_list_append
(
 level0_commandlist_map_entry_t *command_list,
 ze_kernel_handle_t kernel,
 ze_event_handle_t event
)
{
  level0_kernel_list_entry_t* list_entry = level0_commandlist_map_kernel_list_node_new();
  list_entry->kernel = kernel;
  list_entry->event = event;
  list_entry->next = command_list->head;
  command_list->head = list_entry;
}

//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
level0_commandlist_map_count
(
 void
)
{
  return st_count(map_root);
}
