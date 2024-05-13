// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
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

#include "../../../cct/cct.h"
#include "../../../messages/errors.h"

#include "gpu-host-correlation-map.h"
#include "../../gpu-splay-allocator.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "../../common/gpu-print.h"


#define st_insert                               \
  typed_splay_insert(host_correlation)

#define st_lookup                               \
  typed_splay_lookup(host_correlation)

#define st_delete                               \
  typed_splay_delete(host_correlation)

#define st_forall                               \
  typed_splay_forall(host_correlation)

#define st_count                                \
  typed_splay_count(host_correlation)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, gpu_host_correlation_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//******************************************************************************
// type declarations
//******************************************************************************

#undef typed_splay_node
#define typed_splay_node(host_correlation) gpu_host_correlation_map_entry_t

typedef struct typed_splay_node(host_correlation) {
  struct typed_splay_node(host_correlation) *left;
  struct typed_splay_node(host_correlation) *right;

  uint64_t host_correlation_id; // key

  gpu_activity_channel_t *activity_channel;
} typed_splay_node(host_correlation);



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_host_correlation_map_entry_t *map_root = NULL;

static __thread gpu_host_correlation_map_entry_t *free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(host_correlation)


static gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_entry_new
(
 uint64_t host_correlation_id,
 gpu_activity_channel_t *activity_channel
)
{
  gpu_host_correlation_map_entry_t *e = st_alloc(&free_list);

  memset(e, 0, sizeof(gpu_host_correlation_map_entry_t));

  e->host_correlation_id = host_correlation_id;
  e->activity_channel = activity_channel;

  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_lookup
(
 uint64_t host_correlation_id
)
{
  gpu_host_correlation_map_entry_t *result = st_lookup(&map_root, host_correlation_id);

  PRINT("host_correlation_map lookup: id=0x%lx (entry %p) (&map_root=%p) tid=%llu\n",
        host_correlation_id, result, &map_root, (uint64_t) pthread_self());

  return result;
}


void
gpu_host_correlation_map_insert
(
 uint64_t host_correlation_id,
 gpu_activity_channel_t *activity_channel
)
{
  gpu_host_correlation_map_entry_t *entry = st_lookup(&map_root, host_correlation_id);
  if (entry) {
    // fatal error: host_correlation id already present; a
    // correlation should be inserted only once.
    hpcrun_terminate();
  } else {
    gpu_host_correlation_map_entry_t *entry =
      gpu_host_correlation_map_entry_new(host_correlation_id, activity_channel);

    st_insert(&map_root, entry);

    PRINT("host_correlation_map insert: correlation_id=0x%lx "
         "activity_channel=%p (entry=%p) (&map_root=%p) tid=%llu\n",
          host_correlation_id, activity_channel, entry, &map_root,
          (uint64_t) pthread_self());
  }
}


void
gpu_host_correlation_map_delete
(
 uint64_t host_correlation_id
)
{
  PRINT("host_correlation_map delete: correlation_id=0x%lx\n", host_correlation_id);
  gpu_host_correlation_map_entry_t *node = st_delete(&map_root, host_correlation_id);
  st_free(&free_list, node);
}


gpu_activity_channel_t *
gpu_host_correlation_map_entry_channel_get
(
 gpu_host_correlation_map_entry_t *entry
)
{
  return entry->activity_channel;
}
