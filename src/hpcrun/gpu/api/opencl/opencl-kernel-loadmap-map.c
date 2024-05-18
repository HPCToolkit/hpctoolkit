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

#include "../../../../common/lean/splay-uint64.h"
#include "../../../../common/lean/spinlock.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../gpu-splay-allocator.h"
#include "../../activity/gpu-op-placeholders.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../../common/gpu-print.h"


#define st_insert                               \
  typed_splay_insert(correlation_id)

#define st_lookup                               \
  typed_splay_lookup(correlation_id)

#define st_delete                               \
  typed_splay_delete(correlation_id)

#define st_forall                               \
  typed_splay_forall(correlation_id)

#define st_count                                \
  typed_splay_count(correlation_id)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, opencl_kernel_loadmap_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(correlation_id) opencl_kernel_loadmap_map_entry_t

typedef struct typed_splay_node(correlation_id) {
  struct typed_splay_node(correlation_id) *left;
  struct typed_splay_node(correlation_id) *right;
  uint64_t kernel_name_id; // key using kernel name hash

  uint32_t module_id;
} typed_splay_node(correlation_id);



//******************************************************************************
// local data
//******************************************************************************

static opencl_kernel_loadmap_map_entry_t *map_root = NULL;

static opencl_kernel_loadmap_map_entry_t *free_list = NULL;

static spinlock_t opencl_kernel_loadmap_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(correlation_id)


static opencl_kernel_loadmap_map_entry_t *
opencl_kernel_loadmap_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static opencl_kernel_loadmap_map_entry_t *
opencl_kernel_loadmap_map_entry_new
(
 uint64_t kernel_name_id,
 uint32_t module_id
)
{
  opencl_kernel_loadmap_map_entry_t *e = opencl_kernel_loadmap_map_entry_alloc();

  e->kernel_name_id = kernel_name_id;
  e->module_id = module_id;
  return e;
}


//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_kernel_loadmap_map_entry_t *
opencl_kernel_loadmap_map_lookup
(
 uint64_t kernel_name_id
)
{
  spinlock_lock(&opencl_kernel_loadmap_map_lock);

  uint64_t id = kernel_name_id;
  opencl_kernel_loadmap_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&opencl_kernel_loadmap_map_lock);

  return result;
}


void
opencl_kernel_loadmap_map_insert
(
 uint64_t kernel_name_id,
 uint32_t module_id
)
{
  spinlock_lock(&opencl_kernel_loadmap_map_lock);

  opencl_kernel_loadmap_map_entry_t *entry = st_lookup(&map_root, kernel_name_id);
  if (entry) {
    entry->kernel_name_id = kernel_name_id;
    entry->module_id = module_id;
  } else {
    opencl_kernel_loadmap_map_entry_t *entry =
      opencl_kernel_loadmap_map_entry_new(kernel_name_id, module_id);

    st_insert(&map_root, entry);
  }

  spinlock_unlock(&opencl_kernel_loadmap_map_lock);
}


void
opencl_kernel_loadmap_map_delete
(
 uint64_t kernel_name_id
)
{
  spinlock_lock(&opencl_kernel_loadmap_map_lock);

  opencl_kernel_loadmap_map_entry_t *node = st_delete(&map_root, kernel_name_id);
  st_free(&free_list, node);

  spinlock_unlock(&opencl_kernel_loadmap_map_lock);
}


uint64_t
opencl_kernel_loadmap_map_entry_kernel_name_id_get
(
 opencl_kernel_loadmap_map_entry_t *entry
)
{
  return entry->kernel_name_id;
}


uint32_t
opencl_kernel_loadmap_map_entry_module_id_get
(
 opencl_kernel_loadmap_map_entry_t *entry
)
{
  return entry->module_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
opencl_kernel_loadmap_map_count
(
 void
)
{
  return st_count(map_root);
}
