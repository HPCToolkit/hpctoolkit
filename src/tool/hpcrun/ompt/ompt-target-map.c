/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/ompt/ompt-target-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "ompt-cct-node-vector.h"



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_target_map_entry_s {
  cct_node_t *target;  // cupti function id
  uint64_t refcnt;
  ompt_cct_node_vector_t *vector;
  struct ompt_target_map_entry_s *left;
  struct ompt_target_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_target_map_entry_t *ompt_target_map_root = NULL;
static spinlock_t ompt_target_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_target_map_entry_t *
ompt_target_map_entry_new(cct_node_t *target)
{
  ompt_target_map_entry_t *e;
  e = (ompt_target_map_entry_t *)hpcrun_malloc(sizeof(ompt_target_map_entry_t));
  e->target = target;
  e->vector = (ompt_cct_node_vector_t *)ompt_cct_node_vector_init();
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_target_map_entry_t *
ompt_target_map_splay(ompt_target_map_entry_t *root, cct_node_t *key)
{
  REGULAR_SPLAY_TREE(ompt_target_map_entry_s, root, key, target, left, right);
  return root;
}


static void
ompt_target_map_delete_root()
{
  TMSG(DEFER_CTXT, "target %d: delete", ompt_target_map_root->target);

  if (ompt_target_map_root->left == NULL) {
    ompt_target_map_root = ompt_target_map_root->right;
  } else {
    ompt_target_map_root->left = 
      ompt_target_map_splay(ompt_target_map_root->left, 
			   ompt_target_map_root->target);
    ompt_target_map_root->left->right = ompt_target_map_root->right;
    ompt_target_map_root = ompt_target_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_target_map_entry_t *
ompt_target_map_lookup(cct_node_t *target)
{
  ompt_target_map_entry_t *result = NULL;
  spinlock_lock(&ompt_target_map_lock);

  ompt_target_map_root = ompt_target_map_splay(ompt_target_map_root, target);
  if (ompt_target_map_root && ompt_target_map_root->target == target) {
    result = ompt_target_map_root;
  }

  spinlock_unlock(&ompt_target_map_lock);

  TMSG(DEFER_CTXT, "target map lookup: target=0x%lx (record %p)", target, result);
  return result;
}


void
ompt_target_map_insert(cct_node_t *target)
{
  ompt_target_map_entry_t *entry = ompt_target_map_entry_new(target);

  TMSG(DEFER_CTXT, "target map insert: id=%p (record %p)", target, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_target_map_lock);

  if (ompt_target_map_root != NULL) {
    ompt_target_map_root = 
      ompt_target_map_splay(ompt_target_map_root, target);

    if (target < ompt_target_map_root->target) {
      entry->left = ompt_target_map_root->left;
      entry->right = ompt_target_map_root;
      ompt_target_map_root->left = NULL;
    } else if (target > ompt_target_map_root->target) {
      entry->left = ompt_target_map_root;
      entry->right = ompt_target_map_root->right;
      ompt_target_map_root->right = NULL;
    } else {
      // target already present: fatal error since a target 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_target_map_root = entry;

  spinlock_unlock(&ompt_target_map_lock);
}


// return true if record found; false otherwise
bool
ompt_target_map_refcnt_update(cct_node_t *target, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "target map refcnt_update: id=0x%lx (update %d)", 
       target, val);

  spinlock_lock(&ompt_target_map_lock);
  ompt_target_map_root = ompt_target_map_splay(ompt_target_map_root, target);

  if (ompt_target_map_root && 
      ompt_target_map_root->target == target) {
    uint64_t old = ompt_target_map_root->refcnt;
    ompt_target_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "target map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 target, old, ompt_target_map_root->refcnt);
    if (ompt_target_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "target map refcnt_update: id=0x%lx (deleting)",
           target);
      ompt_target_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_target_map_lock);
  return result;
}


void ompt_target_map_child_insert(cct_node_t *target, cct_node_t *cct_node)
{
  ompt_target_map_entry_t *entry = ompt_target_map_lookup(target);
  ompt_cct_node_vector_push_back(entry->vector, cct_node);
}


cct_node_t *ompt_target_map_seq_lookup(cct_node_t *target, uint64_t id)
{
  ompt_target_map_entry_t *entry = ompt_target_map_lookup(target);
  return ompt_cct_node_vector_get(entry->vector, id);
}


uint64_t 
ompt_target_map_entry_refcnt_get(ompt_target_map_entry_t *entry) 
{
  return entry->refcnt;
}



/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_target_map_count_helper(ompt_target_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_target_map_count_helper(entry->left);
     int right = ompt_target_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_target_map_count() 
{
  return ompt_target_map_count_helper(ompt_target_map_root);
}

