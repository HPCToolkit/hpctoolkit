/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <cuda.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-stream-id-map.h"

#define CUPTI_STREAM_ID_MAP_DEBUG 1

#if CUPTI_STREAM_ID_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_stream_id_map_entry_s {
  uint32_t stream_id;
  cupti_trace_t *trace;
  struct cupti_stream_id_map_entry_s *left;
  struct cupti_stream_id_map_entry_s *right;
}; 


/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_stream_id_map_entry_t *
cupti_stream_id_map_splay
(
 cupti_stream_id_map_entry_t *root,
 uint32_t key
)
{
  REGULAR_SPLAY_TREE(cupti_stream_id_map_entry_s, root, key, stream_id, left, right);
  return root;
}


static void
cupti_stream_id_map_delete_root
(
 cupti_stream_id_map_entry_t **root
)
{
  if ((*root)->left == NULL) {
    *root = (*root)->right;
  } else {
    (*root)->left = cupti_stream_id_map_splay((*root)->left, (*root)->stream_id);
    (*root)->left->right = (*root)->right;
    *root = (*root)->left;
  }
}


static void 
cupti_stream_id_map_process_helper
(
 cupti_stream_id_map_entry_t *entry,
 cupti_trace_fn_t fn,
 void *arg
) 
{
  if (entry) {
    fn(entry->trace, arg);
    cupti_stream_id_map_process_helper(entry->left, fn, arg);
    cupti_stream_id_map_process_helper(entry->right, fn, arg);
  } 
}


static void
cupti_stream_id_map_insert
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  cupti_stream_id_map_entry_t *entry = NULL;

  if (*root != NULL) {
    *root = cupti_stream_id_map_splay(*root, stream_id);

    if (stream_id < (*root)->stream_id) {
      entry = cupti_stream_id_map_entry_new(stream_id);
      entry->left = entry->right = NULL;
      entry->left = (*root)->left;
      entry->right = *root;
      (*root)->left = NULL;
      *root = entry;
    } else if (stream_id > (*root)->stream_id) {
      entry = cupti_stream_id_map_entry_new(stream_id);
      entry->left = entry->right = NULL;
      entry->left = *root;
      entry->right = (*root)->right;
      (*root)->right = NULL;
      *root = entry;
    } else {
      // stream_id already present
      entry = (*root);
    }
  } else {
    entry = cupti_stream_id_map_entry_new(stream_id);
    entry->left = entry->right = NULL;
    *root = entry;
  }
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_stream_id_map_entry_t *
cupti_stream_id_map_lookup
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  cupti_stream_id_map_entry_t *result = NULL;

  *root = cupti_stream_id_map_splay(*root, stream_id);
  if ((*root) && (*root)->stream_id == stream_id) {
    result = *root;
  }

  return result;
}


void
cupti_stream_id_map_delete
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id
)
{
  *root = cupti_stream_id_map_splay(*root, stream_id);
  if (*root && (*root)->stream_id == stream_id) {
    cupti_stream_id_map_delete_root(root);
  }
}


void
cupti_stream_id_map_stream_process
(
 cupti_stream_id_map_entry_t **root,
 uint32_t stream_id,
 cupti_trace_fn_t fn,
 void *arg
)
{
  cupti_stream_id_map_insert(root, stream_id);
  fn((*root)->trace, arg); 
}


void
cupti_stream_id_map_context_process
(
 cupti_stream_id_map_entry_t **root,
 cupti_trace_fn_t fn,
 void *arg
)
{
  cupti_stream_id_map_process_helper(*root, fn, arg);
}


cupti_stream_id_map_entry_t *
cupti_stream_id_map_entry_new(uint32_t stream_id)
{
  PRINT("Create a new trace with stream id %u\n", stream_id);

  cupti_stream_id_map_entry_t *e;
  e = (cupti_stream_id_map_entry_t *)
    hpcrun_malloc_safe(sizeof(cupti_stream_id_map_entry_t));
  e->stream_id = stream_id;
  e->trace = cupti_trace_create();
  e->left = NULL;
  e->right = NULL;

  return e;
}
