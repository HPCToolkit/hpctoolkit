/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <openssl/md5.h>  // MD5



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-md5-map.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cubin_md5_map_entry_s {
  uint64_t cubin_id;
  uint64_t refcnt;
  unsigned char md5[MD5_DIGEST_LENGTH];
  struct cubin_md5_map_entry_s *left;
  struct cubin_md5_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cubin_md5_map_entry_t *cubin_md5_map_root = NULL;
static spinlock_t cubin_md5_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cubin_md5_map_entry_t *
cubin_md5_map_entry_new(uint64_t cubin_id, const void *cubin, size_t size)
{
  cubin_md5_map_entry_t *e;
  e = (cubin_md5_map_entry_t *)hpcrun_malloc(sizeof(cubin_md5_map_entry_t));
  e->cubin_id = cubin_id;
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;
  MD5(cubin, size, e->md5);

  return e;
}


static cubin_md5_map_entry_t *
cubin_md5_map_splay(cubin_md5_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(cubin_md5_map_entry_s, root, key, cubin_id, left, right);
  return root;
}


static void
cubin_md5_map_delete_root()
{
  TMSG(DEFER_CTXT, "cubin_id %d: delete", cubin_md5_map_root->cubin_id);

  if (cubin_md5_map_root->left == NULL) {
    cubin_md5_map_root = cubin_md5_map_root->right;
  } else {
    cubin_md5_map_root->left = 
      cubin_md5_map_splay(cubin_md5_map_root->left, 
			   cubin_md5_map_root->cubin_id);
    cubin_md5_map_root->left->right = cubin_md5_map_root->right;
    cubin_md5_map_root = cubin_md5_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_md5_map_entry_t *
cubin_md5_map_lookup(uint64_t id)
{
  cubin_md5_map_entry_t *result = NULL;
  spinlock_lock(&cubin_md5_map_lock);

  cubin_md5_map_root = cubin_md5_map_splay(cubin_md5_map_root, id);
  if (cubin_md5_map_root && cubin_md5_map_root->cubin_id == id) {
    result = cubin_md5_map_root;
  }

  spinlock_unlock(&cubin_md5_map_lock);

  TMSG(DEFER_CTXT, "cubin_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cubin_md5_map_insert(uint64_t cubin_id, const void *cubin, size_t size)
{
  spinlock_lock(&cubin_md5_map_lock);

  if (cubin_md5_map_root != NULL) {
    cubin_md5_map_root = 
      cubin_md5_map_splay(cubin_md5_map_root, cubin_id);

    if (cubin_id < cubin_md5_map_root->cubin_id) {
      cubin_md5_map_entry_t *entry = cubin_md5_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_md5_map_root->left;
      entry->right = cubin_md5_map_root;
      cubin_md5_map_root->left = NULL;
      cubin_md5_map_root = entry;
    } else if (cubin_id > cubin_md5_map_root->cubin_id) {
      cubin_md5_map_entry_t *entry = cubin_md5_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_md5_map_root;
      entry->right = cubin_md5_map_root->right;
      cubin_md5_map_root->right = NULL;
      cubin_md5_map_root = entry;
    } else {
      // cubin_id already present
    }
  } else {
      cubin_md5_map_entry_t *entry = cubin_md5_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      cubin_md5_map_root = entry;
  }

  spinlock_unlock(&cubin_md5_map_lock);
}


// return true if record found; false otherwise
bool
cubin_md5_map_refcnt_update(uint64_t cubin_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "cubin_id map refcnt_update: id=0x%lx (update %d)", 
       cubin_id, val);

  spinlock_lock(&cubin_md5_map_lock);
  cubin_md5_map_root = cubin_md5_map_splay(cubin_md5_map_root, cubin_id);

  if (cubin_md5_map_root && 
      cubin_md5_map_root->cubin_id == cubin_id) {
    uint64_t old = cubin_md5_map_root->refcnt;
    cubin_md5_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "cubin_id map refcnt_update: id=0x%lx (%ld --> %ld)", 
	    cubin_id, old, cubin_md5_map_root->refcnt);
    if (cubin_md5_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "cubin_id map refcnt_update: id=0x%lx (deleting)",
           cubin_id);
      cubin_md5_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&cubin_md5_map_lock);
  return result;
}


uint64_t 
cubin_md5_map_entry_refcnt_get(cubin_md5_map_entry_t *entry) 
{
  return entry->refcnt;
}


unsigned char *
cubin_md5_map_entry_md5_get(cubin_md5_map_entry_t *entry)
{
  return entry->md5;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cubin_md5_map_count_helper(cubin_md5_map_entry_t *entry) 
{
  if (entry) {
     int left = cubin_md5_map_count_helper(entry->left);
     int right = cubin_md5_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cubin_md5_map_count() 
{
  return cubin_md5_map_count_helper(cubin_md5_map_root);
}

