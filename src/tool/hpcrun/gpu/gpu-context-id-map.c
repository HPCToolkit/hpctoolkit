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

#include "gpu-context-id-map.h"
#include "gpu-stream-id-map.h"



//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(context_id)

#define st_lookup				\
  typed_splay_lookup(context_id)

#define st_delete				\
  typed_splay_delete(context_id)

#define st_forall				\
  typed_splay_forall(context_id)

#define st_count				\
  typed_splay_count(context_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(context_id))

#define st_free(free_list, node)		\
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


typedef struct trace_fn_helper_t {
  gpu_trace_fn_t fn;
  void *arg;
} trace_fn_helper_t;



//******************************************************************************
// local data
//******************************************************************************

static gpu_context_id_map_entry_t *map_root = NULL;
static gpu_context_id_map_entry_t *free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(context_id)


static gpu_context_id_map_entry_t *
gpu_context_id_map_entry_new(uint32_t context_id, uint32_t stream_id)
{
  gpu_context_id_map_entry_t *e;
  e = (gpu_context_id_map_entry_t *)
    hpcrun_malloc_safe(sizeof(gpu_context_id_map_entry_t));

  memset(e, 0, sizeof(gpu_context_id_map_entry_t));

  e->context_id = context_id;
  e->streams = gpu_stream_id_map_entry_new(stream_id);

  return e;
}


static void
gpu_context_id_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_context_id_map_entry_t *entry = st_lookup(&map_root, context_id);

  if (entry == NULL) {
    entry = gpu_context_id_map_entry_new(context_id, stream_id);
    st_insert(&map_root, entry);
  }
}


static void
trace_fn_helper
(
 gpu_context_id_map_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  trace_fn_helper_t *info = (trace_fn_helper_t *) arg;
  gpu_stream_id_map_context_process(&entry->streams, info->fn, info->arg);
}


static void
signal_context
(
 gpu_context_id_map_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  gpu_stream_map_signal_all(&entry->streams);
}


static void
gpu_context_id_map_adjust_times
(
 gpu_context_id_map_entry_t *entry,
 gpu_trace_item_t *ti
)
{
  if (entry->first_time == 0) {
    entry->first_time = ti->cpu_submit_time;
    if (ti->start < entry->first_time) {
      entry->time_offset = entry->first_time - ti->start;
    } else {
      entry->time_offset = 0;
    }
  }
  ti->start += entry->time_offset;
  ti->end += entry->time_offset;
}



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


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
)
{
  gpu_context_id_map_entry_t *node = st_delete(&map_root, context_id);
  st_free(&free_list, node);
}


void
gpu_context_id_map_stream_delete
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_context_id_map_entry_t *entry = st_lookup(&map_root, context_id);
  if (entry) {
    gpu_stream_id_map_delete(&(entry->streams), stream_id);
  }
}


void
gpu_context_id_map_stream_process
(
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
)
{
  gpu_context_id_map_insert(context_id, stream_id);
  gpu_context_id_map_adjust_times(map_root, ti);
  gpu_stream_id_map_stream_process(&(map_root->streams), 
				   stream_id, fn, ti);
}


void
gpu_context_id_map_context_process
(
 uint32_t context_id,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
)
{
  gpu_context_id_map_entry_t *entry = gpu_context_id_map_lookup(context_id);
  if (entry != NULL) {
    gpu_context_id_map_adjust_times(entry, ti);
    gpu_stream_id_map_context_process(&(entry->streams), fn, ti);
  }
}


void
gpu_context_id_map_device_process
(
 gpu_trace_fn_t fn,
 void *arg
)
{
  trace_fn_helper_t info;
  info.fn = fn;
  info.arg = arg;
  st_forall(map_root, splay_inorder, trace_fn_helper, &info);
}


void
gpu_context_stream_map_signal_all
(
 void
)
{
  st_forall(map_root, splay_inorder, signal_context, 0);
}

