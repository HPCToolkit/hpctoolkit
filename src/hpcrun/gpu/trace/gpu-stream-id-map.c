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
// Copyright ((c)) 2002-2024, Rice University
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

#define _GNU_SOURCE

#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../common/prof-lean/splay-uint64.h"

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
