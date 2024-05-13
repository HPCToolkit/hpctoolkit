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

#include "../../../common/lean/splay-uint64.h"

#include "../../memory/hpcrun-malloc.h"

#include "gpu-stream-id-map.h"
#include "gpu-trace-channel.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../common/gpu-print.h"


#undef typed_splay_node
#define typed_splay_node(stream_id) gpu_stream_id_map_entry_t


#define st_insert                               \
  typed_splay_insert(stream_id)

#define st_lookup                               \
  typed_splay_lookup(stream_id)

#define st_delete                               \
  typed_splay_delete(stream_id)

#define st_forall                               \
  typed_splay_forall(stream_id)

#define st_count                                \
  typed_splay_count(stream_id)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, typed_splay_node(stream_id))

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct typed_splay_node(stream_id) {
  struct typed_splay_node(stream_id) *left;
  struct typed_splay_node(stream_id) *right;
  uint32_t stream_id;
  gpu_trace_channel_t *channel;
} typed_splay_node(stream_id);


typedef struct iter_fn_helper_data_t {
  void (*iter_fn)(gpu_trace_channel_t *, void *);
  void *arg;
} iter_fn_helper_data_t;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(stream_id)

static void
iter_fn_helper
(
 gpu_stream_id_map_entry_t *node,
 splay_visit_t visit_type,
 void *arg
)
{
  iter_fn_helper_data_t *data = (iter_fn_helper_data_t *) arg;
  data->iter_fn(node->channel, arg);
}



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_stream_id_map_entry_t *
gpu_stream_id_map_lookup
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  return st_lookup(root, stream_id);;
}


void
gpu_stream_id_map_delete
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  gpu_stream_id_map_entry_t *deleted __attribute__((unused));
  deleted = st_delete(root, stream_id);
}


_Bool
gpu_stream_id_map_insert_entry
(
 gpu_stream_id_map_entry_t **root,
 gpu_stream_id_map_entry_t *entry
)
{
  return st_insert(root, entry);
}


void
gpu_stream_id_map_for_each
(
 gpu_stream_id_map_entry_t **root,
 void (*iter_fn)(gpu_trace_channel_t *, void *),
 void *arg
)
{
  iter_fn_helper_data_t data = {
    .iter_fn = iter_fn,
    .arg = arg
  };

  st_forall(*root, splay_inorder, iter_fn_helper, &data);
}


//*****************************************************************************
// entry interface operations
//*****************************************************************************

gpu_stream_id_map_entry_t *
gpu_stream_id_map_entry_new
(
 uint32_t stream_id
)
{
  gpu_stream_id_map_entry_t *entry = hpcrun_malloc_safe(sizeof(gpu_stream_id_map_entry_t));

  memset(entry, 0, sizeof(gpu_stream_id_map_entry_t));

  entry->stream_id = stream_id;
  entry->channel = NULL;

  return entry;
}


void
gpu_stream_id_map_entry_set_channel
(
  gpu_stream_id_map_entry_t *entry,
  gpu_trace_channel_t *channel
)
{
  entry->channel = channel;
}


gpu_trace_channel_t *
gpu_stream_id_map_entry_get_channel
(
  gpu_stream_id_map_entry_t *entry
)
{
  return entry->channel;
}
