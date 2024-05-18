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

#include "../../../../common/lean/splay-uint64.h"
#include "../../../../common/lean/spinlock.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../gpu-splay-allocator.h"
#include "../../activity/gpu-op-placeholders.h"

#include "opencl-queue-map.h"


//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../../common/gpu-print.h"


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
  typed_splay_alloc(free_list, opencl_queue_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) opencl_queue_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t queue; // key

  uint32_t queue_id; // we save queue id as the stream id
  uint32_t context_id;
} typed_splay_node(queue);


//******************************************************************************
// local data
//******************************************************************************

static opencl_queue_map_entry_t *map_root = NULL;

static opencl_queue_map_entry_t *free_list = NULL;

static spinlock_t opencl_queue_map_lock = SPINLOCK_UNLOCKED;

static uint32_t cl_queue_id = 0;

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static opencl_queue_map_entry_t *
opencl_cl_queue_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static opencl_queue_map_entry_t *
opencl_cl_queue_map_entry_new
(
 uint64_t queue,
 uint32_t context_id
)
{
  opencl_queue_map_entry_t *e = opencl_cl_queue_map_entry_alloc();

  e->queue = queue;
  e->queue_id = cl_queue_id;
  e->context_id = context_id;

  return e;
}


//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_queue_map_entry_t *
opencl_cl_queue_map_lookup
(
 uint64_t queue
)
{
  spinlock_lock(&opencl_queue_map_lock);

  uint64_t id = queue;
  opencl_queue_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&opencl_queue_map_lock);

  return result;
}


uint32_t
opencl_cl_queue_map_update
(
 uint64_t queue,
 uint32_t context_id
)
{
  uint32_t ret_queue_id = 0;

  spinlock_lock(&opencl_queue_map_lock);

  opencl_queue_map_entry_t *entry = st_lookup(&map_root, queue);
  if (entry) {
    entry->queue = queue;
    entry->queue_id = cl_queue_id;
    entry->context_id = context_id;
  } else {
    opencl_queue_map_entry_t *entry =
      opencl_cl_queue_map_entry_new(queue, context_id);

    st_insert(&map_root, entry);
  }

  // Update cl_queue_id
  ret_queue_id = cl_queue_id++;

  spinlock_unlock(&opencl_queue_map_lock);

  return ret_queue_id;
}


void
opencl_cl_queue_map_delete
(
 uint64_t queue
)
{
  spinlock_lock(&opencl_queue_map_lock);

  opencl_queue_map_entry_t *node = st_delete(&map_root, queue);
  st_free(&free_list, node);

  spinlock_unlock(&opencl_queue_map_lock);
}


uint32_t
opencl_cl_queue_map_entry_queue_id_get
(
 opencl_queue_map_entry_t *entry
)
{
  return entry->queue_id;
}


uint32_t
opencl_cl_queue_map_entry_context_id_get
(
 opencl_queue_map_entry_t *entry
)
{
  return entry->context_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
opencl_cl_queue_map_count
(
 void
)
{
  return st_count(map_root);
}
