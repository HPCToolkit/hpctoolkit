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
// Copyright ((c)) 2002-2021, Rice University
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

#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-stream-id-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "gpu-print.h"


#undef typed_splay_node
#define typed_splay_node(stream_id) gpu_stream_id_map_entry_t


#define st_insert				\
  typed_splay_insert(stream_id)

#define st_lookup				\
  typed_splay_lookup(stream_id)

#define st_delete				\
  typed_splay_delete(stream_id)

#define st_forall				\
  typed_splay_forall(stream_id)

#define st_count				\
  typed_splay_count(stream_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(stream_id))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct typed_splay_node(stream_id) {
  struct typed_splay_node(stream_id) *left;
  struct typed_splay_node(stream_id) *right;
  uint32_t stream_id;
  gpu_trace_t *trace;
} typed_splay_node(stream_id);


typedef struct trace_fn_helper_t {
  gpu_trace_fn_t fn;
  gpu_trace_item_t *ti;
} trace_fn_helper_t;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(stream_id)


static void
gpu_stream_id_map_insert
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  if (st_lookup(root, stream_id) == NULL) {
    // stream_id is not already present
    gpu_stream_id_map_entry_t *entry = gpu_stream_id_map_entry_new(stream_id);
    st_insert(root, entry);
  }
}


static void
signal_stream
(
 gpu_stream_id_map_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  gpu_trace_signal_consumer(entry->trace);
}


static void
trace_fn_helper
(
 gpu_stream_id_map_entry_t *node,
 splay_visit_t visit_type,
 void *arg
)
{
  trace_fn_helper_t *info = (trace_fn_helper_t *) arg;
  info->fn(node->trace, info->ti);
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
  gpu_stream_id_map_entry_t *result = NULL;

  *root = st_lookup(root, stream_id);

  return result;
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


void
gpu_stream_id_map_stream_process
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
)
{
  gpu_stream_id_map_insert(root, stream_id);
  fn((*root)->trace, ti);
}


gpu_stream_id_map_entry_t *
gpu_stream_id_map_entry_new(uint32_t stream_id)
{
  PRINT("Create a new trace with stream id %u\n", stream_id);

  gpu_stream_id_map_entry_t *e = (gpu_stream_id_map_entry_t *)
    hpcrun_malloc_safe(sizeof(gpu_stream_id_map_entry_t));

  memset(e, 0, sizeof(gpu_stream_id_map_entry_t));

  e->stream_id = stream_id;
  e->trace = gpu_trace_create();

  return e;
}


void
gpu_stream_id_map_context_process
(
 gpu_stream_id_map_entry_t **root,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
)
{
  trace_fn_helper_t info;
  info.fn = fn;
  info.ti = ti;
  st_forall(*root, splay_inorder, trace_fn_helper, &info);
}


void
gpu_stream_map_signal_all
(
 gpu_stream_id_map_entry_t **root
)
{
  st_forall(*root, splay_inorder, signal_stream, 0);
}

