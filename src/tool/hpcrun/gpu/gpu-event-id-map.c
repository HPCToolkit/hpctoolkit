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
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <string.h>


//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-event-id-map.h"
#include "gpu-splay-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "gpu-print.h"

#define st_insert				\
  typed_splay_insert(event_id)

#define st_lookup				\
  typed_splay_lookup(event_id)

#define st_delete				\
  typed_splay_delete(event_id)

#define st_forall				\
  typed_splay_forall(event_id)

#define st_count				\
  typed_splay_count(event_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_event_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)


//******************************************************************************
// type declarations
//******************************************************************************
#undef typed_splay_node
#define typed_splay_node(correlation_id) gpu_event_id_map_entry_t

typedef struct typed_splay_node(event_id) { 
  struct gpu_event_id_map_entry_t *left;
  struct gpu_event_id_map_entry_t *right;
  uint64_t event_id;

  uint32_t context_id;
  uint32_t stream_id;
} typed_splay_node(event_id); 


//******************************************************************************
// local data
//******************************************************************************

static gpu_event_id_map_entry_t *map_root = NULL;
static gpu_event_id_map_entry_t *free_list = NULL;

//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(event_id)


static gpu_event_id_map_entry_t *
gpu_event_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gpu_event_id_map_entry_t *
gpu_event_id_map_entry_new(uint32_t event_id, uint32_t context_id, uint32_t stream_id)
{
  gpu_event_id_map_entry_t *e = gpu_event_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_event_id_map_entry_t));

  e->event_id = event_id;
  e->context_id = context_id;
  e->stream_id = stream_id;

  return e;
}


//******************************************************************************
// interface operations
//******************************************************************************

gpu_event_id_map_entry_t *
gpu_event_id_map_lookup
(
 uint32_t event_id
)
{
  gpu_event_id_map_entry_t *result = st_lookup(&map_root, event_id);

  TMSG(DEFER_CTXT, "event map lookup: event=0x%lx (record %p)", 
       event_id, result);

  return result;
}


void
gpu_event_id_map_insert
(
 uint32_t event_id,
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_event_id_map_entry_t *entry = gpu_event_id_map_lookup(event_id);

  if (entry != NULL) {
    // Update current event_id related context and stream
    // If a event is recorded twice, use the later one
    // A common pattern:
    // cuEventRecord(a);
    // cuLaunchKernel();
    // cuEventRecord(b);
    // cuEventSynchronize(a);
    // --- We get events in sequence from the buffer, now we can ignore the first record of a
    // cuEventRecord(a);
    entry->context_id = context_id;
    entry->stream_id = stream_id;
  } else {
    entry = gpu_event_id_map_entry_new(event_id, context_id, stream_id);

    st_insert(&map_root, entry);

    PRINT("event_id_map insert: event_id=0x%lx\n", event_id);
  }
}


void
gpu_event_id_map_delete
(
 uint32_t event_id
)
{
  gpu_event_id_map_entry_t *node = st_delete(&map_root, event_id);
  st_free(&free_list, node);
}


uint32_t
gpu_event_id_map_entry_stream_id_get
(
 gpu_event_id_map_entry_t *entry
)
{
  return entry->stream_id;
}


uint32_t
gpu_event_id_map_entry_context_id_get
(
 gpu_event_id_map_entry_t *entry
)
{
  return entry->context_id;
}
