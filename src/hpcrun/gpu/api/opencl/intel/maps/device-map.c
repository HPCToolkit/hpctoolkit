// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <string.h>                       // memset


//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../../../../common/lean/splay-uint64.h"
#include "../../../../../../common/lean/spinlock.h"
#include "../../../../gpu-splay-allocator.h" // typed_splay_alloc
#include "device-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#define st_insert                               \
  typed_splay_insert(queue)

#define st_lookup                               \
  typed_splay_lookup(queue)

#define st_delete                               \
  typed_splay_delete(queue)

#define st_forall                               \
  typed_splay_forall(queue)

#define st_count                                \
  typed_splay_count(queue)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, device_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) device_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t device_id; // key

} typed_splay_node(queue);


//******************************************************************************
// local data
//******************************************************************************

static device_map_entry_t *map_root = NULL;
static device_map_entry_t *free_list = NULL;

static spinlock_t device_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static device_map_entry_t *
device_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static device_map_entry_t *
device_map_entry_new
(
 uint64_t device_id
)
{
  device_map_entry_t *e = device_map_entry_alloc();

  e->device_id = device_id;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

device_map_entry_t *
device_map_lookup
(
 uint64_t device_id
)
{
  spinlock_lock(&device_map_lock);

  uint64_t id = device_id;
  device_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&device_map_lock);

  return result;
}


// called on clEnqueueReadBuffer and clEnqueueWriteBuffer
bool
device_map_insert
(
 uint64_t device_id
)
{
  spinlock_lock(&device_map_lock);
  bool new_entry = false;

  device_map_entry_t *entry = st_lookup(&map_root, device_id);
  if (!entry) {
    new_entry = true;
    entry = device_map_entry_new(device_id);
    st_insert(&map_root, entry);
  }

  spinlock_unlock(&device_map_lock);
  return new_entry;
}


void
device_map_delete
(
 uint64_t device_id
)
{
  spinlock_lock(&device_map_lock);

  device_map_entry_t *node = st_delete(&map_root, device_id);
  st_free(&free_list, node);

  spinlock_unlock(&device_map_lock);
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
device_map_count
(
 void
)
{
  return st_count(map_root);
}
