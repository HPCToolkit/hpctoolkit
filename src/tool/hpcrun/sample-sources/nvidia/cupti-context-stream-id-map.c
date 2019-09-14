//
// Created by ax4 on 8/1/19.
//

#include "cupti-context-stream-id-map.h"


#define CUPTI_CONTEXT_STREAM_ID_MAP_DEBUG 0

#if CUPTI_CONTEXT_STREAM_ID_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif


struct cupti_context_stream_id_map_entry_s {
  uint64_t context_stream_id;
  cupti_trace_t *trace;
  struct cupti_context_stream_id_map_entry_s *left;
  struct cupti_context_stream_id_map_entry_s *right;
};

struct cupti_context_stream_id_map_entry_s *cupti_context_stream_id_map_root = NULL;


static uint64_t
cupti_context_stream_id_generate
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  return ((uint64_t)context_id << 32) | (uint64_t)stream_id;
}


static cupti_context_stream_id_map_entry_t *
cupti_context_stream_id_map_entry_new
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  PRINT("Create new thread <context_id %u, stream_id %u>\n", context_id, stream_id);

  cupti_context_stream_id_map_entry_t *e;
  e = (cupti_context_stream_id_map_entry_t *)hpcrun_malloc_safe(sizeof(cupti_context_stream_id_map_entry_t));
  e->context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);
  e->trace = cupti_trace_create();
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_context_stream_id_map_entry_t *
cupti_context_stream_id_map_splay
(
 cupti_context_stream_id_map_entry_t *root,
 uint64_t key
)
{
  REGULAR_SPLAY_TREE(cupti_context_stream_id_map_entry_s, root, key, context_stream_id, left, right);
  return root;
}


static void
cupti_context_stream_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "context_stream_id %d: delete", cupti_context_stream_id_map_root->context_stream_id);

  if (cupti_context_stream_id_map_root->left == NULL) {
    cupti_context_stream_id_map_root = cupti_context_stream_id_map_root->right;
  } else {
    cupti_context_stream_id_map_root->left =
      cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root->left,
        cupti_context_stream_id_map_root->context_stream_id);
    cupti_context_stream_id_map_root->left->right = cupti_context_stream_id_map_root->right;
    cupti_context_stream_id_map_root = cupti_context_stream_id_map_root->left;
  }
}


cupti_context_stream_id_map_entry_t *
cupti_context_stream_id_map_lookup
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);

  cupti_context_stream_id_map_entry_t *result = NULL;

  cupti_context_stream_id_map_root = cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);
  if (cupti_context_stream_id_map_root && cupti_context_stream_id_map_root->context_stream_id == context_stream_id) {
    result = cupti_context_stream_id_map_root;
  }

  TMSG(DEFER_CTXT, "context_stream_id map lookup: id=0x%lx (record %p)", context_stream_id, result);
  return result;
}


void
cupti_context_stream_id_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);

  cupti_context_stream_id_map_entry_t *entry = cupti_context_stream_id_map_entry_new(context_id, stream_id);

  TMSG(DEFER_CTXT, "context_stream_id map insert: id=0x%lx (record %p)", context_stream_id, entry);

  entry->left = entry->right = NULL;

  if (cupti_context_stream_id_map_root != NULL) {
    cupti_context_stream_id_map_root =
      cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);

    if (context_stream_id < cupti_context_stream_id_map_root->context_stream_id) {
      entry->left = cupti_context_stream_id_map_root->left;
      entry->right = cupti_context_stream_id_map_root;
      cupti_context_stream_id_map_root->left = NULL;
    } else if (context_stream_id > cupti_context_stream_id_map_root->context_stream_id) {
      entry->left = cupti_context_stream_id_map_root;
      entry->right = cupti_context_stream_id_map_root->right;
      cupti_context_stream_id_map_root->right = NULL;
    } else {
      // correlation_id already present: fatal error since a correlation_id
      //   should only be inserted once
      assert(0);
    }
  }

  cupti_context_stream_id_map_root = entry;
}


void
cupti_context_stream_id_map_delete
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  uint64_t context_stream_id = cupti_context_stream_id_generate(context_id, stream_id);
  cupti_context_stream_id_map_root =
    cupti_context_stream_id_map_splay(cupti_context_stream_id_map_root, context_stream_id);

  if (cupti_context_stream_id_map_root &&
    cupti_context_stream_id_map_root->context_stream_id == context_stream_id) {
    cupti_context_stream_id_map_delete_root();
  }
}


void
cupti_context_stream_id_map_append
(
 uint32_t context_id,
 uint32_t stream_id,
 uint64_t start,
 uint64_t end,
 cct_node_t *cct_node
)
{
  // TODO(Keren): refactor
  cupti_context_stream_id_map_entry_t *entry = cupti_context_stream_id_map_lookup(context_id, stream_id);
  if (entry == NULL) {
    cupti_context_stream_id_map_insert(context_id, stream_id);
    entry = cupti_context_stream_id_map_lookup(context_id, stream_id);
  }
  cupti_trace_append(entry->trace, start, end, cct_node);
}


static void
cupti_context_stream_id_map_process_helper
(
 cupti_context_stream_id_map_entry_t *entry,
 cupti_context_stream_id_map_fn_t fn
)
{
  if (entry) {
    fn(entry->trace);
    cupti_context_stream_id_map_process_helper(entry->left, fn);
    cupti_context_stream_id_map_process_helper(entry->right, fn);
    return;
  }
}


void
cupti_context_stream_id_map_process
(
 cupti_context_stream_id_map_fn_t fn
)
{
  cupti_context_stream_id_map_process_helper(cupti_context_stream_id_map_root, fn);
}
