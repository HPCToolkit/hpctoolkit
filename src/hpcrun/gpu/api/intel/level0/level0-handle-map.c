// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
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

#include "../../../../../common/lean/splay-uint64.h"
#include "../../../../../common/lean/spinlock.h"
#include "../../../../memory/hpcrun-malloc.h"
#include "../../../gpu-splay-allocator.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../../../common/gpu-print.h"


#define st_insert                               \
  typed_splay_insert(handle)

#define st_lookup                               \
  typed_splay_lookup(handle)

#define st_delete                               \
  typed_splay_delete(handle)

#define st_forall                               \
  typed_splay_forall(handle)

#define st_count                                \
  typed_splay_count(handle)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, level0_handle_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(handle) level0_handle_map_entry_t

typedef struct typed_splay_node(handle) {
  struct typed_splay_node(handle) *left;
  struct typed_splay_node(handle) *right;
  uint64_t level0_handle; // key : can be any level0 handle
  level0_data_node_t* data; // value: can also be any pointer type
} typed_splay_node(handle);

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(handle)


static level0_handle_map_entry_t *
level0_handle_map_entry_alloc(level0_handle_map_entry_t **free_list_ptr)
{
  return st_alloc(free_list_ptr);
}

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_handle_map_entry_t *
level0_handle_map_lookup
(
 level0_handle_map_entry_t** map_root_ptr,
 uint64_t key
)
{
  level0_handle_map_entry_t *result = st_lookup(map_root_ptr, key);
  return result;
}

void
level0_handle_map_insert
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t* new_entry
)
{
  st_insert(map_root_ptr, new_entry);
}

void
level0_handle_map_delete
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t** free_list_ptr,
 uint64_t key
)
{
  level0_handle_map_entry_t *node = st_delete(map_root_ptr, key);
  st_free(free_list_ptr, node);
}

level0_handle_map_entry_t*
level0_handle_map_entry_new
(
 level0_handle_map_entry_t **free_list,
 uint64_t key,
 level0_data_node_t* data
)
{
  level0_handle_map_entry_t *e = level0_handle_map_entry_alloc(free_list);

  memset(e, 0, sizeof(level0_handle_map_entry_t));
  e->level0_handle = key;
  e->data = data;

  return e;
}

level0_data_node_t**
level0_handle_map_entry_data_get
(
 level0_handle_map_entry_t *entry
)
{
  return &(entry->data);
}
