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
#include "cupti-correlation-id-map.h"



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_correlation_id_map_entry_s {
  uint64_t correlation_id;
  uint64_t refcnt;
  uint64_t external_id;
  struct ompt_correlation_id_map_entry_s *left;
  struct ompt_correlation_id_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_correlation_id_map_entry_t *ompt_correlation_id_map_root = NULL;
static spinlock_t ompt_correlation_id_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_correlation_id_map_entry_t *
ompt_correlation_id_map_entry_new(uint64_t correlation_id, uint64_t external_id)
{
  ompt_correlation_id_map_entry_t *e;
  e = (ompt_correlation_id_map_entry_t *)hpcrun_malloc(sizeof(ompt_correlation_id_map_entry_t));
  e->correlation_id = correlation_id;
  e->refcnt = 0;
  e->external_id = external_id;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_correlation_id_map_entry_t *
ompt_correlation_id_map_splay(ompt_correlation_id_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_correlation_id_map_entry_s, root, key, correlation_id, left, right);
  return root;
}


static void
ompt_correlation_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "correlation_id %d: delete", ompt_correlation_id_map_root->correlation_id);

  if (ompt_correlation_id_map_root->left == NULL) {
    ompt_correlation_id_map_root = ompt_correlation_id_map_root->right;
  } else {
    ompt_correlation_id_map_root->left = 
      ompt_correlation_id_map_splay(ompt_correlation_id_map_root->left, 
			   ompt_correlation_id_map_root->correlation_id);
    ompt_correlation_id_map_root->left->right = ompt_correlation_id_map_root->right;
    ompt_correlation_id_map_root = ompt_correlation_id_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_correlation_id_map_entry_t *
ompt_correlation_id_map_lookup(uint64_t id)
{
  ompt_correlation_id_map_entry_t *result = NULL;
  spinlock_lock(&ompt_correlation_id_map_lock);

  ompt_correlation_id_map_root = ompt_correlation_id_map_splay(ompt_correlation_id_map_root, id);
  if (ompt_correlation_id_map_root && ompt_correlation_id_map_root->correlation_id == id) {
    result = ompt_correlation_id_map_root;
  }

  spinlock_unlock(&ompt_correlation_id_map_lock);

  TMSG(DEFER_CTXT, "correlation_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_correlation_id_map_insert(uint64_t correlation_id, uint64_t external_id)
{
  ompt_correlation_id_map_entry_t *entry = ompt_correlation_id_map_entry_new(correlation_id, external_id);

  TMSG(DEFER_CTXT, "correlation_id map insert: id=0x%lx (record %p)", correlation_id, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_correlation_id_map_lock);

  if (ompt_correlation_id_map_root != NULL) {
    ompt_correlation_id_map_root = 
      ompt_correlation_id_map_splay(ompt_correlation_id_map_root, correlation_id);

    if (correlation_id < ompt_correlation_id_map_root->correlation_id) {
      entry->left = ompt_correlation_id_map_root->left;
      entry->right = ompt_correlation_id_map_root;
      ompt_correlation_id_map_root->left = NULL;
    } else if (correlation_id > ompt_correlation_id_map_root->correlation_id) {
      entry->left = ompt_correlation_id_map_root;
      entry->right = ompt_correlation_id_map_root->right;
      ompt_correlation_id_map_root->right = NULL;
    } else {
      // correlation_id already present: fatal error since a correlation_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_correlation_id_map_root = entry;

  spinlock_unlock(&ompt_correlation_id_map_lock);
}


// return true if record found; false otherwise
bool
ompt_correlation_id_map_refcnt_update(uint64_t correlation_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "correlation_id map refcnt_update: id=0x%lx (update %d)", 
       correlation_id, val);

  spinlock_lock(&ompt_correlation_id_map_lock);
  ompt_correlation_id_map_root = ompt_correlation_id_map_splay(ompt_correlation_id_map_root, correlation_id);

  if (ompt_correlation_id_map_root && 
      ompt_correlation_id_map_root->correlation_id == correlation_id) {
    uint64_t old = ompt_correlation_id_map_root->refcnt;
    ompt_correlation_id_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "correlation_id map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 correlation_id, old, ompt_correlation_id_map_root->refcnt);
    if (ompt_correlation_id_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "correlation_id map refcnt_update: id=0x%lx (deleting)",
           correlation_id);
      ompt_correlation_id_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_correlation_id_map_lock);
  return result;
}


uint64_t 
ompt_correlation_id_map_entry_refcnt_get(ompt_correlation_id_map_entry_t *entry) 
{
  return entry->refcnt;
}


uint64_t
ompt_correlation_id_map_entry_external_id_get(ompt_correlation_id_map_entry_t *entry)
{
  return entry->external_id;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_correlation_id_map_count_helper(ompt_correlation_id_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_correlation_id_map_count_helper(entry->left);
     int right = ompt_correlation_id_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_correlation_id_map_count() 
{
  return ompt_correlation_id_map_count_helper(ompt_correlation_id_map_root);
}


