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
#include "ompt-device-map.h"



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_device_map_entry_s {
  uint64_t device_id;
  uint64_t refcnt;
  ompt_device_t *device;
  char *type;
  struct ompt_device_map_entry_s *left;
  struct ompt_device_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_device_map_entry_t *ompt_device_map_root = NULL;
static spinlock_t ompt_device_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_device_map_entry_t *
ompt_device_map_entry_new(uint64_t device_id, ompt_device_t *device, char *type)
{
  ompt_device_map_entry_t *e;
  e = (ompt_device_map_entry_t *)hpcrun_malloc(sizeof(ompt_device_map_entry_t));
  e->device_id = device_id;
  e->device = device;
  e->type = type;
  e->left = NULL;
  e->right = NULL;
  e->refcnt = 0;

  return e;
}


static ompt_device_map_entry_t *
ompt_device_map_splay(ompt_device_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_device_map_entry_s, root, key, device_id, left, right);
  return root;
}


static void
ompt_device_map_delete_root()
{
  TMSG(DEFER_CTXT, "device %d: delete", ompt_device_map_root->device_id);

  if (ompt_device_map_root->left == NULL) {
    ompt_device_map_root = ompt_device_map_root->right;
  } else {
    ompt_device_map_root->left = 
      ompt_device_map_splay(ompt_device_map_root->left, 
			   ompt_device_map_root->device_id);
    ompt_device_map_root->left->right = ompt_device_map_root->right;
    ompt_device_map_root = ompt_device_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_device_map_entry_t *
ompt_device_map_lookup(uint64_t id)
{
  ompt_device_map_entry_t *result = NULL;
  spinlock_lock(&ompt_device_map_lock);

  ompt_device_map_root = ompt_device_map_splay(ompt_device_map_root, id);
  if (ompt_device_map_root && ompt_device_map_root->device_id == id) {
    result = ompt_device_map_root;
  }

  spinlock_unlock(&ompt_device_map_lock);

  TMSG(DEFER_CTXT, "device map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_device_map_insert(uint64_t device_id, ompt_device_t *device, char *type)
{
  ompt_device_map_entry_t *entry = ompt_device_map_entry_new(device_id, device, type);

  TMSG(DEFER_CTXT, "device map insert: id=0x%lx (record %p)", device_id, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_device_map_lock);

  if (ompt_device_map_root != NULL) {
    ompt_device_map_root = 
      ompt_device_map_splay(ompt_device_map_root, device_id);

    if (device_id < ompt_device_map_root->device_id) {
      entry->left = ompt_device_map_root->left;
      entry->right = ompt_device_map_root;
      ompt_device_map_root->left = NULL;
    } else if (device_id > ompt_device_map_root->device_id) {
      entry->left = ompt_device_map_root;
      entry->right = ompt_device_map_root->right;
      ompt_device_map_root->right = NULL;
    } else {
      // device_id already present: fatal error since a device_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_device_map_root = entry;

  spinlock_unlock(&ompt_device_map_lock);
}


// return true if record found; false otherwise
bool
ompt_device_map_refcnt_update(uint64_t device_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (update %d)", 
       device_id, val);

  spinlock_lock(&ompt_device_map_lock);
  ompt_device_map_root = ompt_device_map_splay(ompt_device_map_root, device_id);

  if (ompt_device_map_root && 
      ompt_device_map_root->device_id == device_id) {
    uint64_t old = ompt_device_map_root->refcnt;
    ompt_device_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (%ld --> %ld)",
      device_id, old, ompt_device_map_root->refcnt);
    if (ompt_device_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (deleting)",
        device_id);
      ompt_device_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_device_map_lock);
  return result;
}


ompt_device_t *
ompt_device_map_entry_device_get(ompt_device_map_entry_t *entry)
{
  return entry->device;
}



/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_device_map_count_helper(ompt_device_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_device_map_count_helper(entry->left);
     int right = ompt_device_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_device_map_count() 
{
  return ompt_device_map_count_helper(ompt_device_map_root);
}


