/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/crypto-hash.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-hash-map.h"



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cubin_hash_map_entry_s {
  const void *cubin;
  struct cubin_hash_map_entry_s *left;
  struct cubin_hash_map_entry_s *right;
  unsigned char hash[0];
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static cubin_hash_map_entry_t *cubin_hash_map_root = NULL;
static spinlock_t cubin_hash_map_lock = SPINLOCK_UNLOCKED;


/******************************************************************************
 * private operations
 *****************************************************************************/

static cubin_hash_map_entry_t *
cubin_hash_map_entry_new(const void *cubin, size_t size)
{
  cubin_hash_map_entry_t *e;
  unsigned int hash_length = crypto_hash_length();
  e = (cubin_hash_map_entry_t *)
  hpcrun_malloc_safe(sizeof(cubin_hash_map_entry_t) + hash_length);
  e->cubin_id = cubin_id;
  e->left = NULL;
  e->right = NULL;
  crypto_hash_compute(cubin, size, e->hash, hash_length);

  return e;
}


static cubin_hash_map_entry_t *
cubin_hash_map_splay(cubin_hash_map_entry_t *root, const void *key)
{
  REGULAR_SPLAY_TREE(cubin_hash_map_entry_s, root, key, cubin, left, right);
  return root;
}


static void
cubin_hash_map_delete_root()
{
  TMSG(DEFER_CTXT, "cubin %p: delete", cubin_hash_map_root->cubin);

  if (cubin_hash_map_root->left == NULL) {
    cubin_hash_map_root = cubin_hash_map_root->right;
  } else {
    cubin_hash_map_root->left = 
      cubin_hash_map_splay(cubin_hash_map_root->left, 
			   cubin_hash_map_root->cubin);
    cubin_hash_map_root->left->right = cubin_hash_map_root->right;
    cubin_hash_map_root = cubin_hash_map_root->left;
  }
}


void
cubin_hash_map_delete
(
 const void *cubin
)
{
  spinlock_lock(&cubin_hash_map_lock);

  cubin_hash_map_root = cubin_hash_map_splay(cubin_hash_map_root, cubin);
  if (cubin_hash_map_root && cubin_hash_map_root->cubin == cubin) {
    cubin_hash_map_delete_root();
  }

  spinlock_unlock(&cubin_hash_map_lock);
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

cubin_hash_map_entry_t *
cubin_hash_map_lookup(const void *cubin)
{
  cubin_hash_map_entry_t *result = NULL;
  spinlock_lock(&cubin_hash_map_lock);

  cubin_hash_map_root = cubin_hash_map_splay(cubin_hash_map_root, cubin);
  if (cubin_hash_map_root && cubin_hash_map_root->cubin == cubin) {
    result = cubin_hash_map_root;
  }

  spinlock_unlock(&cubin_hash_map_lock);

  TMSG(DEFER_CTXT, "cubin map lookup: cubin=0x%lx (record %p)", cubin, result);
  return result;
}


void
cubin_hash_map_insert(const void *cubin, size_t size)
{
  spinlock_lock(&cubin_hash_map_lock);

  if (cubin_hash_map_root != NULL) {
    cubin_hash_map_root = 
      cubin_hash_map_splay(cubin_hash_map_root, cubin);

    if (cubin < cubin_hash_map_root->cubin) {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_hash_map_root->left;
      entry->right = cubin_hash_map_root;
      cubin_hash_map_root->left = NULL;
      cubin_hash_map_root = entry;
    } else if (cubin > cubin_hash_map_root->cubin) {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_hash_map_root;
      entry->right = cubin_hash_map_root->right;
      cubin_hash_map_root->right = NULL;
      cubin_hash_map_root = entry;
    } else {
      // cubin already present
    }
  } else {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin, size);
      entry->left = entry->right = NULL;
      cubin_hash_map_root = entry;
  }

  spinlock_unlock(&cubin_hash_map_lock);
}


unsigned char *
cubin_hash_map_entry_hash_get(cubin_hash_map_entry_t *entry, unsigned int *len)
{
  *len = crypto_hash_length();
  return entry->hash;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cubin_hash_map_count_helper(cubin_hash_map_entry_t *entry) 
{
  if (entry) {
     int left = cubin_hash_map_count_helper(entry->left);
     int right = cubin_hash_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}

int 
cubin_hash_map_count() 
{
  return cubin_hash_map_count_helper(cubin_hash_map_root);
}

