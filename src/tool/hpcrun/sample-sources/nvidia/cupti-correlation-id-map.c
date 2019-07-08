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

#include "cupti-correlation-id-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_correlation_id_map_entry_s {
  uint32_t correlation_id;
  uint64_t external_id;
  uint32_t device_id;
  uint64_t start;
  uint64_t end;
  struct cupti_correlation_id_map_entry_s *left;
  struct cupti_correlation_id_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_correlation_id_map_entry_t *cupti_correlation_id_map_root = NULL;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_correlation_id_map_entry_t *
cupti_correlation_id_map_entry_new(uint32_t correlation_id, uint64_t external_id)
{
  cupti_correlation_id_map_entry_t *e;
  e = (cupti_correlation_id_map_entry_t *)hpcrun_malloc_safe(sizeof(cupti_correlation_id_map_entry_t));
  e->correlation_id = correlation_id;
  e->external_id = external_id;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_correlation_id_map_entry_t *
cupti_correlation_id_map_splay(cupti_correlation_id_map_entry_t *root, uint32_t key)
{
  REGULAR_SPLAY_TREE(cupti_correlation_id_map_entry_s, root, key, correlation_id, left, right);
  return root;
}


static void
cupti_correlation_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "correlation_id %d: delete", cupti_correlation_id_map_root->correlation_id);

  if (cupti_correlation_id_map_root->left == NULL) {
    cupti_correlation_id_map_root = cupti_correlation_id_map_root->right;
  } else {
    cupti_correlation_id_map_root->left = 
      cupti_correlation_id_map_splay(cupti_correlation_id_map_root->left, 
			   cupti_correlation_id_map_root->correlation_id);
    cupti_correlation_id_map_root->left->right = cupti_correlation_id_map_root->right;
    cupti_correlation_id_map_root = cupti_correlation_id_map_root->left;
  }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_correlation_id_map_entry_t *
cupti_correlation_id_map_lookup(uint32_t id)
{
  cupti_correlation_id_map_entry_t *result = NULL;

  cupti_correlation_id_map_root = cupti_correlation_id_map_splay(cupti_correlation_id_map_root, id);
  if (cupti_correlation_id_map_root && cupti_correlation_id_map_root->correlation_id == id) {
    result = cupti_correlation_id_map_root;
  }

  TMSG(DEFER_CTXT, "correlation_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cupti_correlation_id_map_insert(uint32_t correlation_id, uint64_t external_id)
{
  cupti_correlation_id_map_entry_t *entry = cupti_correlation_id_map_entry_new(correlation_id, external_id);

  TMSG(DEFER_CTXT, "correlation_id map insert: id=0x%lx (record %p)", correlation_id, entry);

  entry->left = entry->right = NULL;

  if (cupti_correlation_id_map_root != NULL) {
    cupti_correlation_id_map_root = 
      cupti_correlation_id_map_splay(cupti_correlation_id_map_root, correlation_id);

    if (correlation_id < cupti_correlation_id_map_root->correlation_id) {
      entry->left = cupti_correlation_id_map_root->left;
      entry->right = cupti_correlation_id_map_root;
      cupti_correlation_id_map_root->left = NULL;
    } else if (correlation_id > cupti_correlation_id_map_root->correlation_id) {
      entry->left = cupti_correlation_id_map_root;
      entry->right = cupti_correlation_id_map_root->right;
      cupti_correlation_id_map_root->right = NULL;
    } else {
      // correlation_id already present: fatal error since a correlation_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  cupti_correlation_id_map_root = entry;
}


// TODO(Keren): remove
void
cupti_correlation_id_map_external_id_replace(uint32_t correlation_id, uint64_t external_id)
{
  TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

  cupti_correlation_id_map_root =
    cupti_correlation_id_map_splay(cupti_correlation_id_map_root, correlation_id);

  if (cupti_correlation_id_map_root &&
    cupti_correlation_id_map_root->correlation_id == correlation_id) {
    cupti_correlation_id_map_root->external_id = external_id;
  }
}


void
cupti_correlation_id_map_delete(uint32_t correlation_id)
{
  cupti_correlation_id_map_root =
    cupti_correlation_id_map_splay(cupti_correlation_id_map_root, correlation_id);

  if (cupti_correlation_id_map_root &&
    cupti_correlation_id_map_root->correlation_id == correlation_id) {
    cupti_correlation_id_map_delete_root();
  }
}


void
cupti_correlation_id_map_kernel_update
(
 uint32_t correlation_id,
 uint32_t device_id,
 uint64_t start,
 uint64_t end
)
{
  TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

  cupti_correlation_id_map_root =
    cupti_correlation_id_map_splay(cupti_correlation_id_map_root, correlation_id);

  if (cupti_correlation_id_map_root &&
    cupti_correlation_id_map_root->correlation_id == correlation_id) {
    cupti_correlation_id_map_root->device_id = device_id;
    cupti_correlation_id_map_root->start = start;
    cupti_correlation_id_map_root->end = end;
  }
}


uint64_t
cupti_correlation_id_map_entry_external_id_get(cupti_correlation_id_map_entry_t *entry)
{
  return entry->external_id;
}


uint64_t
cupti_correlation_id_map_entry_start_get(cupti_correlation_id_map_entry_t *entry)
{
  return entry->start;
}


uint64_t
cupti_correlation_id_map_entry_end_get(cupti_correlation_id_map_entry_t *entry)
{
  return entry->end;
}


uint32_t
cupti_correlation_id_map_entry_device_id_get(cupti_correlation_id_map_entry_t *entry)
{
  return entry->device_id;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_correlation_id_map_count_helper(cupti_correlation_id_map_entry_t *entry) 
{
  if (entry) {
     int left = cupti_correlation_id_map_count_helper(entry->left);
     int right = cupti_correlation_id_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_correlation_id_map_count() 
{
  return cupti_correlation_id_map_count_helper(cupti_correlation_id_map_root);
}
