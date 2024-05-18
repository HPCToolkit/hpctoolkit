// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <string.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../common/lean/splay-uint64.h"
#include "../../../messages/messages.h"

#include "../../gpu-splay-allocator.h"

#include "gpu-function-id-map.h"

//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "../../common/gpu-print.h"


#define st_insert                               \
  typed_splay_insert(function_id)

#define st_lookup                               \
  typed_splay_lookup(function_id)

#define st_delete                               \
  typed_splay_delete(function_id)

#define st_forall                               \
  typed_splay_forall(function_id)

#define st_count                                \
  typed_splay_count(function_id)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, typed_splay_node(function_id))

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//******************************************************************************
// type declarations
//******************************************************************************

#undef typed_splay_node
#define typed_splay_node(function_id) gpu_function_id_map_entry_t

typedef struct typed_splay_node(function_id) {
  struct typed_splay_node(function_id) *left;
  struct typed_splay_node(function_id) *right;

  uint64_t function_id; // key
  ip_normalized_t pc;
} typed_splay_node(function_id);


//******************************************************************************
// local data
//******************************************************************************

static gpu_function_id_map_entry_t *map_root = NULL;

static gpu_function_id_map_entry_t *free_list = NULL;


//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(function_id)


static gpu_function_id_map_entry_t *
gpu_function_id_map_entry_alloc
(
 void
)
{
  return st_alloc(&free_list);
}


static gpu_function_id_map_entry_t *
gpu_function_id_map_entry_new
(
 uint64_t function_id,
 ip_normalized_t pc
)
{
  gpu_function_id_map_entry_t *e = gpu_function_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_function_id_map_entry_t));

  e->function_id = function_id;
  e->pc = pc;

  return e;
}


//******************************************************************************
// interface operations
//******************************************************************************

gpu_function_id_map_entry_t *
gpu_function_id_map_lookup
(
 uint64_t function_id
)
{
  gpu_function_id_map_entry_t *result = st_lookup(&map_root, function_id);

  PRINT("function_id_map lookup: id=0x%lx (entry %p)", function_id, result);

  return result;
}


void
gpu_function_id_map_insert
(
 uint64_t function_id,
 ip_normalized_t pc
)
{
  if (st_lookup(&map_root, function_id)) {
    // fatal error: function_id already present; a
    // correlation should be inserted only once.
    hpcrun_terminate();
  } else {
    gpu_function_id_map_entry_t *entry =
      gpu_function_id_map_entry_new(function_id, pc);

    st_insert(&map_root, entry);
  }
}


void
gpu_function_id_map_delete
(
 uint64_t function_id
)
{
  gpu_function_id_map_entry_t *node = st_delete(&map_root, function_id);
  st_free(&free_list, node);
}


ip_normalized_t
gpu_function_id_map_entry_pc_get
(
 gpu_function_id_map_entry_t *entry
)
{
  return entry->pc;
}

//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gpu_function_id_map_count
(
 void
)
{
  return st_count(map_root);
}
