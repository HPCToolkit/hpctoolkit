/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-context-id-map.h"
#include "cupti-stream-id-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_context_id_map_entry_s {
  uint32_t context_id;
  cupti_stream_id_map_entry_t *streams;
  struct cupti_context_id_map_entry_s *left;
  struct cupti_context_id_map_entry_s *right;
}; 


/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_context_id_map_entry_t *cupti_context_id_map_root = NULL;


/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_context_id_map_entry_t *
cupti_context_id_map_entry_new(uint32_t context_id, uint32_t stream_id)
{
  cupti_context_id_map_entry_t *e;
  e = (cupti_context_id_map_entry_t *)
    hpcrun_malloc_safe(sizeof(cupti_context_id_map_entry_t));
  e->context_id = context_id;
  e->streams = cupti_stream_id_map_entry_new(stream_id);
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_context_id_map_entry_t *
cupti_context_id_map_splay(cupti_context_id_map_entry_t *root, uint32_t key)
{
  REGULAR_SPLAY_TREE(cupti_context_id_map_entry_s, root, key, context_id, left, right);
  return root;
}


static void
cupti_context_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "context %p: delete", cupti_context_id_map_root->context_id);

  if (cupti_context_id_map_root->left == NULL) {
    cupti_context_id_map_root = cupti_context_id_map_root->right;
  } else {
    cupti_context_id_map_root->left = 
      cupti_context_id_map_splay(cupti_context_id_map_root->left, 
        cupti_context_id_map_root->context_id);
    cupti_context_id_map_root->left->right = cupti_context_id_map_root->right;
    cupti_context_id_map_root = cupti_context_id_map_root->left;
  }
}


static void
cupti_context_id_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  cupti_context_id_map_entry_t *entry = NULL;

  if (cupti_context_id_map_root != NULL) {
    cupti_context_id_map_root = cupti_context_id_map_splay(cupti_context_id_map_root, context_id);

    if (context_id < cupti_context_id_map_root->context_id) {
      entry = cupti_context_id_map_entry_new(context_id, stream_id);
      entry->left = entry->right = NULL;
      entry->left = cupti_context_id_map_root->left;
      entry->right = cupti_context_id_map_root;
      cupti_context_id_map_root->left = NULL;
      cupti_context_id_map_root = entry;
    } else if (context_id > cupti_context_id_map_root->context_id) {
      entry = cupti_context_id_map_entry_new(context_id, stream_id);
      entry->left = entry->right = NULL;
      entry->left = cupti_context_id_map_root;
      entry->right = cupti_context_id_map_root->right;
      cupti_context_id_map_root->right = NULL;
      cupti_context_id_map_root = entry;
    } else {
      // context_id already present
      entry = cupti_context_id_map_root;
    }
  } else {
    entry = cupti_context_id_map_entry_new(context_id, stream_id);
    entry->left = entry->right = NULL;
    cupti_context_id_map_root = entry;
  }
}


static void 
cupti_context_id_map_process_helper
(
 cupti_context_id_map_entry_t *entry,
 cupti_trace_fn_t fn,
 void *arg
) 
{
  if (entry) {
    cupti_stream_id_map_context_process(&entry->streams, fn, arg);
    cupti_context_id_map_process_helper(entry->left, fn, arg);
    cupti_context_id_map_process_helper(entry->right, fn, arg);
  } 
}


/******************************************************************************
 * interface operations
 *****************************************************************************/


cupti_context_id_map_entry_t *
cupti_context_id_map_lookup(uint32_t context_id)
{
  cupti_context_id_map_entry_t *result = NULL;
  cupti_context_id_map_root = cupti_context_id_map_splay(cupti_context_id_map_root, context_id);
  if (cupti_context_id_map_root && cupti_context_id_map_root->context_id == context_id) {
    result = cupti_context_id_map_root;
  }

  TMSG(DEFER_CTXT, "context map lookup: context=0x%lx (record %p)", context_id, result);
  return result;
}


void
cupti_context_id_map_context_delete
(
 uint32_t context_id
)
{
  cupti_context_id_map_root = cupti_context_id_map_splay(cupti_context_id_map_root, context_id);
  if (cupti_context_id_map_root && cupti_context_id_map_root->context_id == context_id) {
    cupti_context_id_map_delete_root();
  }
}


void
cupti_context_id_map_stream_delete
(
 uint32_t context_id,
 uint32_t stream_id
)
{
  cupti_context_id_map_root = cupti_context_id_map_splay(cupti_context_id_map_root, context_id);
  if (cupti_context_id_map_root && cupti_context_id_map_root->context_id == context_id) {
    cupti_stream_id_map_delete(&(cupti_context_id_map_root->streams), stream_id);
  }
}


void
cupti_context_id_map_stream_process
(
 uint32_t context_id,
 uint32_t stream_id,
 cupti_trace_fn_t fn,
 void *arg
)
{
  cupti_context_id_map_insert(context_id, stream_id);
  cupti_stream_id_map_stream_process(&(cupti_context_id_map_root->streams), stream_id, fn, arg);
}


void
cupti_context_id_map_context_process
(
 uint32_t context_id,
 cupti_trace_fn_t fn,
 void *arg
)
{
  cupti_context_id_map_entry_t *entry = cupti_context_id_map_lookup(context_id);
  if (entry != NULL) {
    cupti_stream_id_map_context_process(&(entry->streams), fn, arg);
  }
}


void
cupti_context_id_map_device_process
(
 cupti_trace_fn_t fn,
 void *arg
)
{
  cupti_context_id_map_process_helper(cupti_context_id_map_root, fn, arg);
}
