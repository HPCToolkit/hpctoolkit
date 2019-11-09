//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include "gpu-context-stream-id-map.h"
#include "gpu-splay-allocator.h"
#include "gpu-trace-channel.h"

// #include "stream-tracing.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif


#define st_insert				\
  typed_splay_insert(context_stream_id)

#define st_lookup				\
  typed_splay_lookup(context_stream_id)

#define st_delete				\
  typed_splay_delete(context_stream_id)

#define st_forall				\
  typed_splay_forall(context_stream_id)

#define st_count				\
  typed_splay_count(context_stream_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_context_stream_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//******************************************************************************
// type declarations
//******************************************************************************

#undef typed_splay_node
#define typed_splay_node(context_stream_id) gpu_context_stream_id_map_entry_t

typedef struct typed_splay_node(context_stream_id) {
  struct typed_splay_node(context_stream_id) *left;
  struct typed_splay_node(context_stream_id) *right;
  uint64_t context_stream_id; // key
  gpu_trace_channel_t *trace_channel;
} gpu_context_stream_id_map_entry_t;



//******************************************************************************
// local variables
//******************************************************************************

static gpu_context_stream_id_map_entry_t *map_root = NULL;

static gpu_context_stream_id_map_entry_t *free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(context_stream_id)


static void
signal_node_helper
(
 gpu_context_stream_id_map_entry_t *node,
 splay_visit_t visit_type,
 void *arg
)
{
  gpu_trace_channel_signal_consumer(node->trace_channel);
}


static uint64_t
stream_id_generate
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  return ((uint64_t)context_id << 32)| (uint64_t)stream_id;
}

static gpu_context_stream_id_map_entry_t *
gpu_context_stream_id_map_entry_alloc
(
 void
)
{
  return st_alloc(&free_list);
}


static gpu_context_stream_id_map_entry_t *
gpu_context_stream_id_map_entry_new
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_context_stream_id_map_entry_t *e = gpu_context_stream_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_context_stream_id_map_entry_t));

  e->context_stream_id = stream_id_generate(context_id, stream_id);

  e->trace_channel = gpu_trace_channel_alloc();

  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

gpu_context_stream_id_map_entry_t *
gpu_context_stream_map_lookup
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  uint64_t context_stream_id = stream_id_generate(context_id, stream_id);

  gpu_context_stream_id_map_entry_t *result = st_lookup(&map_root, context_stream_id);

  TMSG(DEFER_CTXT, "context_stream_id map lookup: id=0x%lx (record %p)", 
       context_stream_id, result);

  return result;
}


void
gpu_context_stream_id_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  gpu_context_stream_id_map_entry_t *entry = 
    gpu_context_stream_id_map_entry_new(context_id, stream_id);

  PRINT("context_stream_id map insert: id=0x%lx (entry %p)", 
       entry->context_stream_id, entry);

  st_insert(&map_root, entry);
}


void
gpu_context_stream_id_map_delete
(
 uint32_t context_id,
 uint32_t stream_id
 )
{
  uint64_t context_stream_id = 
    stream_id_generate(context_id, stream_id);

  gpu_context_stream_id_map_entry_t *entry = st_delete(&map_root, context_stream_id);

  if (entry) st_free(&free_list, entry);
}


void 
gpu_context_stream_map_signal_all
(
 void
) 
{
  st_forall(map_root, splay_inorder, signal_node_helper, 0);
}


//------------------------------------------------------------------------------
// field accessors
//------------------------------------------------------------------------------

gpu_trace_channel_t *
gpu_context_stream_map_trace_channel_get
(
 gpu_context_stream_id_map_entry_t *entry
)
{
  return entry->trace_channel;
}



