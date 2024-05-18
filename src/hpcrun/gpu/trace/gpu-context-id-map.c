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

#include "../../../common/lean/splay-uint64.h"

#include "../../messages/messages.h"
#include "../../memory/hpcrun-malloc.h"

#include "../gpu-splay-allocator.h"

#include "gpu-context-id-map.h"
#include "gpu-stream-id-map.h"
#include "gpu-trace-item.h"



//******************************************************************************
// macros
//******************************************************************************

#define st_insert                               \
  typed_splay_insert(context_id)

#define st_lookup                               \
  typed_splay_lookup(context_id)

#define st_delete                               \
  typed_splay_delete(context_id)

#define st_forall                               \
  typed_splay_forall(context_id)

#define st_count                                \
  typed_splay_count(context_id)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, typed_splay_node(context_id))

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(context_id) gpu_context_id_map_entry_t



//******************************************************************************
// type declarations
//******************************************************************************

struct gpu_context_id_map_entry_t {
  struct gpu_context_id_map_entry_t *left;
  struct gpu_context_id_map_entry_t *right;
  uint64_t context_id;
  uint64_t first_time;
  uint64_t time_offset;
  gpu_stream_id_map_entry_t *streams;
};



//******************************************************************************
// local data
//******************************************************************************

static gpu_context_id_map_entry_t *map_root = NULL;
static gpu_context_id_map_entry_t *free_list = NULL;


static bool has_gpu_timestamp = false;
static uint64_t cpu_timestamp;
static uint64_t gpu_timestamp;



//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(context_id)



//******************************************************************************
// interface operations
//******************************************************************************

gpu_context_id_map_entry_t *
gpu_context_id_map_lookup
(
 uint32_t context_id
)
{
  gpu_context_id_map_entry_t *result = st_lookup(&map_root, context_id);

  TMSG(DEFER_CTXT, "context map lookup: context=0x%lx (record %p)",
       context_id, result);

  return result;
}


_Bool
gpu_context_id_map_insert_entry
(
 gpu_context_id_map_entry_t *entry
)
{
  return st_insert(&map_root, entry);
}


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
)
{
  gpu_context_id_map_entry_t *node = st_delete(&map_root, context_id);
  st_free(&free_list, node);
}


// This is a workaround interface to align CPU & GPU timestamps.
// Currently, only rocm support uses this.
void
gpu_context_id_map_set_cpu_gpu_timestamp
(
  uint64_t t1,
  uint64_t t2
)
{
  if (has_gpu_timestamp) return;
  has_gpu_timestamp = true;
  cpu_timestamp = t1;
  gpu_timestamp = t2;
}


void
gpu_context_id_map_adjust_times
(
 gpu_context_id_map_entry_t *entry,
 gpu_trace_item_t *ti
)
{
  // If we have established correspondence between
  // a cpu timestamp and a gpu timestamp, we just need
  // to compute its difference as the time offset
  //
  // TODO: Currently, only rocm support uses this functionality
  // and we have one CPU & GPU timestamp per process.
  // If one process is going to use multiple GPUs and if
  // the timestamps of these GPUs are not synchronized,
  // we will need to store one gpu timestamp per GPU.
  // For rocm, we seem to have one unified HSA timestamp.
  if (has_gpu_timestamp) {
    entry->time_offset = cpu_timestamp - gpu_timestamp;
  } else {
    // Otherwise, we heuristically use the GPU API entry timestamp
    // on the host side to align the CPU and GPU timestamp.
    // This heuristic can cause dramatic CPU & GPU trace misalignment
    // when GPU operations are asynchronous.
    // For example, when we launch a kernel asynchronously,
    // the GPU kernel may not be executed until the runtime loads a GPU binary
    // or performs JIT compilation. In such cases, the host timestamp taken at the
    // entry of the kernel launch API can be significantly earlier than the time
    // when the kernel is actually executed on the GPU.
    if (entry->first_time == 0) {
      entry->first_time = ti->cpu_submit_time;
      if (ti->start < entry->first_time) {
        entry->time_offset = entry->first_time - ti->start;
      } else {
        entry->time_offset = 0;
      }
    }
  }
  ti->start += entry->time_offset;
  ti->end += entry->time_offset;
}


//*****************************************************************************
// entry interface operations
//*****************************************************************************

gpu_context_id_map_entry_t *
gpu_context_id_map_entry_new
(
 uint32_t context_id
)
{
  gpu_context_id_map_entry_t *entry;
  entry = st_alloc(&free_list);

  memset(entry, 0, sizeof(gpu_context_id_map_entry_t));

  entry->context_id = context_id;
  entry->first_time = 0;
  entry->time_offset = 0;

  entry->streams = NULL;

  return entry;
}


gpu_stream_id_map_entry_t **
gpu_context_id_map_entry_get_streams
(
  gpu_context_id_map_entry_t *entry
)
{
  return &entry->streams;
}
