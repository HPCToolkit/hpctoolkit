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
  if (st_lookup(&map_root, event_id)) { 
    assert(0);
  } else {
    gpu_event_id_map_entry_t *entry = gpu_event_id_map_entry_new(event_id, context_id, stream_id);

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
