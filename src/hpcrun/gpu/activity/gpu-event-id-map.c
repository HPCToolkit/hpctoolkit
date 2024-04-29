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

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../common/prof-lean/collections/splay-tree-entry-data.h"
#include "../../../common/prof-lean/collections/freelist-entry-data.h"

#include "../../messages/messages.h"
#include "../../memory/hpcrun-malloc.h"

#include "gpu-event-id-map.h"

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// generic code - splay tree
//******************************************************************************

typedef struct gpu_event_id_map_entry_t {
  union {
    SPLAY_TREE_ENTRY_DATA(struct gpu_event_id_map_entry_t);
    FREELIST_ENTRY_DATA(struct gpu_event_id_map_entry_t);
  };
  uint64_t event_id;

  gpu_event_id_map_entry_value_t value;
} gpu_event_id_map_entry_t;


#define SPLAY_TREE_PREFIX         st
#define SPLAY_TREE_KEY_TYPE       uint64_t
#define SPLAY_TREE_KEY_FIELD      event_id
#define SPLAY_TREE_ENTRY_TYPE     gpu_event_id_map_entry_t
#include "../../../common/prof-lean/collections/splay-tree.h"


#define FREELIST_ENTRY_TYPE       gpu_event_id_map_entry_t
#include "../../../common/prof-lean/collections/freelist.h"



//******************************************************************************
// local data
//******************************************************************************

static __thread st_t event_map = SPLAY_TREE_INITIALIZER;
static __thread freelist_t freelist = FREELIST_INIITALIZER(hpcrun_malloc_safe);



//******************************************************************************
// private operations
//******************************************************************************

static gpu_event_id_map_entry_t *
gpu_event_id_map_entry_new
(
 uint32_t event_id,
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_event_id_map_entry_t *e = freelist_allocate(&freelist);

  memset(e, 0, sizeof(gpu_event_id_map_entry_t));

  e->event_id = event_id;
  e->value.context_id = context_id;
  e->value.context_id = stream_id;

  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

gpu_event_id_map_entry_value_t *
gpu_event_id_map_lookup
(
 uint32_t event_id
)
{
  gpu_event_id_map_entry_t *result = st_lookup(&event_map, event_id);

  TMSG(DEFER_CTXT, "event map lookup: event=0x%lx (record %p)",
       event_id, result);

  return result != NULL ? &result->value : NULL;
}


void
gpu_event_id_map_insert
(
 uint32_t event_id,
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_event_id_map_entry_value_t *entry = gpu_event_id_map_lookup(event_id);

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
    st_insert(&event_map,
              gpu_event_id_map_entry_new(event_id, context_id, stream_id));

    PRINT("event_id_map insert: event_id=0x%lx\n", event_id);
  }
}


void
gpu_event_id_map_delete
(
 uint32_t event_id
)
{
  gpu_event_id_map_entry_t *entry = st_delete(&event_map, event_id);
  freelist_free(&freelist, entry);
}
