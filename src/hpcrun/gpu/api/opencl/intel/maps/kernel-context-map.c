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

#include "../../../../../../common/lean/splay-uint64.h"
#include "../../../../../../common/lean/spinlock.h"
#include "../../../../gpu-splay-allocator.h"
#include "../../../../../memory/hpcrun-malloc.h"    // hpcrun_malloc_safe

#include "kernel-context-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#define st_insert                               \
  typed_splay_insert(context)

#define st_lookup                               \
  typed_splay_lookup(context)

#define st_delete                               \
  typed_splay_delete(context)

#define st_forall                               \
  typed_splay_forall(context)

#define st_count                                \
  typed_splay_count(context)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, kernel_context_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(context) kernel_context_map_entry_t

typedef struct typed_splay_node(context) {
  struct typed_splay_node(context) *left;
  struct typed_splay_node(context) *right;
  uint64_t kernel_id; // key

  uint64_t context_id;
} typed_splay_node(context);


//******************************************************************************
// local data
//******************************************************************************

static kernel_context_map_entry_t *map_root = NULL;
static kernel_context_map_entry_t *free_list = NULL;
static spinlock_t kernel_context_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(context)


static kernel_context_map_entry_t *
kernel_context_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static kernel_context_map_entry_t *
kernel_context_map_entry_new
(
 uint64_t kernel_id
)
{
  kernel_context_map_entry_t *e = kernel_context_map_entry_alloc();
  e->kernel_id = kernel_id;
  e->context_id = 0;
  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

kernel_context_map_entry_t *
kernel_context_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_context_map_lock);

  uint64_t id = kernel_id;
  kernel_context_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&kernel_context_map_lock);

  return result;
}


// called on clEnqueueNDRangeKernel
void
kernel_context_map_insert
(
 uint64_t kernel_id,
 uint64_t context_id
)
{
  spinlock_lock(&kernel_context_map_lock);

  kernel_context_map_entry_t *entry = st_lookup(&map_root, kernel_id);
  if (!entry) {
    entry = kernel_context_map_entry_new(kernel_id);
    st_insert(&map_root, entry);
    entry->context_id = context_id;
  }
  spinlock_unlock(&kernel_context_map_lock);
}


// called on clReleaseKernel
void
kernel_context_map_delete
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_context_map_lock);

  kernel_context_map_entry_t *node = st_delete(&map_root, kernel_id);

  if (!node) {
    // if st_delete returns a null node, that means clReleaseKernel gets called without a call to clEnqueueNDRangeKernel
    return;
  }
  st_free(&free_list, node);

  spinlock_unlock(&kernel_context_map_lock);
}


uint64_t
kernel_context_map_entry_kernel_id_get
(
 kernel_context_map_entry_t *entry
)
{
  return entry->kernel_id;
}


uint64_t
kernel_context_map_entry_context_id_get
(
 kernel_context_map_entry_t *entry
)
{
  return entry->context_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
kernel_context_map_count
(
 void
)
{
  return st_count(map_root);
}
