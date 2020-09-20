/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "sanitizer-op-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct sanitizer_op_map_entry_s {
  int32_t persistent_id;
  cct_node_t *op;
  struct sanitizer_op_map_entry_s *left;
  struct sanitizer_op_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static __thread sanitizer_op_map_entry_t *sanitizer_op_map_root = NULL;


/******************************************************************************
 * private operations
 *****************************************************************************/

static sanitizer_op_map_entry_t *
sanitizer_op_map_entry_new(int32_t persistent_id, cct_node_t *op)
{
  sanitizer_op_map_entry_t *e;
  e = (sanitizer_op_map_entry_t *)
    hpcrun_malloc(sizeof(sanitizer_op_map_entry_t));
  e->persistent_id = persistent_id;
  e->op = op;

  return e;
}


static sanitizer_op_map_entry_t *
sanitizer_op_map_splay(sanitizer_op_map_entry_t *root, int32_t key)
{
  REGULAR_SPLAY_TREE(sanitizer_op_map_entry_s, root, key, persistent_id, left, right);
  return root;
}


static void
sanitizer_op_map_delete_root()
{
  TMSG(DEFER_CTXT, "persistent_id %p: delete", sanitizer_op_map_root->persistent_id);

  if (sanitizer_op_map_root->left == NULL) {
    sanitizer_op_map_root = sanitizer_op_map_root->right;
  } else {
    sanitizer_op_map_root->left = 
      sanitizer_op_map_splay(sanitizer_op_map_root->left, 
			   sanitizer_op_map_root->persistent_id);
    sanitizer_op_map_root->left->right = sanitizer_op_map_root->right;
    sanitizer_op_map_root = sanitizer_op_map_root->left;
  }
}


static sanitizer_op_map_entry_t *
sanitizer_op_map_init_internal(int32_t persistent_id, cct_node_t *op)
{
  sanitizer_op_map_entry_t *entry = NULL;

  if (sanitizer_op_map_root != NULL) {
    sanitizer_op_map_root = 
      sanitizer_op_map_splay(sanitizer_op_map_root, persistent_id);

    if (persistent_id < sanitizer_op_map_root->persistent_id) {
      entry = sanitizer_op_map_entry_new(persistent_id, op);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_op_map_root->left;
      entry->right = sanitizer_op_map_root;
      sanitizer_op_map_root->left = NULL;
      sanitizer_op_map_root = entry;
    } else if (persistent_id > sanitizer_op_map_root->persistent_id) {
      entry = sanitizer_op_map_entry_new(persistent_id, op);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_op_map_root;
      entry->right = sanitizer_op_map_root->right;
      sanitizer_op_map_root->right = NULL;
      sanitizer_op_map_root = entry;
    } else {
      // op already present
      entry = sanitizer_op_map_root;
    }
  } else {
    entry = sanitizer_op_map_entry_new(persistent_id, op);
    entry->left = entry->right = NULL;
    sanitizer_op_map_root = entry;
  }

  return entry;
}


static sanitizer_op_map_entry_t *
sanitizer_op_map_lookup_internal(int32_t persistent_id)
{
  sanitizer_op_map_entry_t *result = NULL;
  sanitizer_op_map_root = sanitizer_op_map_splay(sanitizer_op_map_root, persistent_id);
  if (sanitizer_op_map_root && sanitizer_op_map_root->persistent_id == persistent_id) {
    result = sanitizer_op_map_root;
  }

  TMSG(DEFER_CTXT, "op map lookup: persistent_id=0x%lx (record %p)", persistent_id, result);
  return result;
}

/******************************************************************************
 * interface operations
 *****************************************************************************/


sanitizer_op_map_entry_t *
sanitizer_op_map_lookup(int32_t persistent_id)
{
  sanitizer_op_map_entry_t *result = sanitizer_op_map_lookup_internal(persistent_id);

  return result;
}


sanitizer_op_map_entry_t *
sanitizer_op_map_init(int32_t persistent_id, cct_node_t *op)
{
  sanitizer_op_map_entry_t *result = sanitizer_op_map_init_internal(persistent_id, op);
  
  return result;
}


void
sanitizer_op_map_delete(int32_t persistent_id)
{
  sanitizer_op_map_root = sanitizer_op_map_splay(sanitizer_op_map_root, persistent_id);
  
  if (sanitizer_op_map_root && sanitizer_op_map_root->persistent_id == persistent_id) {
    sanitizer_op_map_delete_root();
  }
}


cct_node_t *
sanitizer_op_map_op_get
(
 sanitizer_op_map_entry_t *entry
)
{
  if (entry != NULL) {
    return entry->op;
  }
  return NULL;
}
