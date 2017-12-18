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
#include "ompt-stop-map.h"



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_stop_map_entry_s {
  bool *stop_flag;
  uint64_t refcnt;
  struct ompt_stop_map_entry_s *left;
  struct ompt_stop_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_stop_map_entry_t *ompt_stop_map_root = NULL;
static spinlock_t ompt_stop_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_stop_map_entry_t *
ompt_stop_map_entry_new(bool *stop_flag)
{
  ompt_stop_map_entry_t *e;
  e = (ompt_stop_map_entry_t *)hpcrun_malloc(sizeof(ompt_stop_map_entry_t));
  e->stop_flag = stop_flag;
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_stop_map_entry_t *
ompt_stop_map_splay(ompt_stop_map_entry_t *root, bool *key)
{
  REGULAR_SPLAY_TREE(ompt_stop_map_entry_s, root, key, stop_flag, left, right);
  return root;
}


static void
ompt_stop_map_delete_root()
{
  TMSG(DEFER_CTXT, "region %d: delete", ompt_stop_map_root->stop_flag);

  if (ompt_stop_map_root->left == NULL) {
    ompt_stop_map_root = ompt_stop_map_root->right;
  } else {
    ompt_stop_map_root->left = 
      ompt_stop_map_splay(ompt_stop_map_root->left, 
			   ompt_stop_map_root->stop_flag);
    ompt_stop_map_root->left->right = ompt_stop_map_root->right;
    ompt_stop_map_root = ompt_stop_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_stop_map_entry_t *
ompt_stop_map_lookup(bool *stop_flag)
{
  ompt_stop_map_entry_t *result = NULL;
  spinlock_lock(&ompt_stop_map_lock);

  ompt_stop_map_root = ompt_stop_map_splay(ompt_stop_map_root, stop_flag);
  if (ompt_stop_map_root && ompt_stop_map_root->stop_flag == stop_flag) {
    result = ompt_stop_map_root;
  }

  spinlock_unlock(&ompt_stop_map_lock);

  TMSG(DEFER_CTXT, "region map lookup: stop_flag=0x%lx (record %p)", stop_flag, result);
  return result;
}


void
ompt_stop_map_insert(bool *stop_flag)
{
  ompt_stop_map_entry_t *entry = ompt_stop_map_entry_new(stop_flag);

  TMSG(DEFER_CTXT, "region map insert: stop_flag=0x%lx (record %p)", stop_flag, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_stop_map_lock);

  if (ompt_stop_map_root != NULL) {
    ompt_stop_map_root = 
      ompt_stop_map_splay(ompt_stop_map_root, stop_flag);

    if (stop_flag < ompt_stop_map_root->stop_flag) {
      entry->left = ompt_stop_map_root->left;
      entry->right = ompt_stop_map_root;
      ompt_stop_map_root->left = NULL;
    } else if (stop_flag > ompt_stop_map_root->stop_flag) {
      entry->left = ompt_stop_map_root;
      entry->right = ompt_stop_map_root->right;
      ompt_stop_map_root->right = NULL;
    } else {
      // stop_flag already present: fatal error since a stop_flag 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_stop_map_root = entry;

  spinlock_unlock(&ompt_stop_map_lock);
}


// return true if record found; false otherwise
bool
ompt_stop_map_refcnt_update(bool *stop_flag, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "region map refcnt_update: stop_flag=0x%lx (update %d)", 
       stop_flag, val);

  spinlock_lock(&ompt_stop_map_lock);
  ompt_stop_map_root = ompt_stop_map_splay(ompt_stop_map_root, stop_flag);

  if (ompt_stop_map_root && 
      ompt_stop_map_root->stop_flag == stop_flag) {
    uint64_t old = ompt_stop_map_root->refcnt;
    ompt_stop_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "region map refcnt_update: stop_flag=0x%lx (%ld --> %ld)", 
	 stop_flag, old, ompt_stop_map_root->refcnt);
    if (ompt_stop_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "region map refcnt_update: stop_flag=0x%lx (deleting)",
           stop_flag);
      ompt_stop_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_stop_map_lock);
  return result;
}


uint64_t 
ompt_stop_map_entry_refcnt_get(ompt_stop_map_entry_t *entry) 
{
  return entry->refcnt;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_stop_map_count_helper(ompt_stop_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_stop_map_count_helper(entry->left);
     int right = ompt_stop_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_stop_map_count() 
{
  return ompt_stop_map_count_helper(ompt_stop_map_root);
}


