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

#include "sanitizer-kernel-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct sanitizer_kernel_map_entry_s {
  cct_node_t * kernel;
  struct sanitizer_kernel_map_entry_s *left;
  struct sanitizer_kernel_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static __thread sanitizer_kernel_map_entry_t *sanitizer_kernel_map_root = NULL;


/******************************************************************************
 * private operations
 *****************************************************************************/

static sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_entry_new(cct_node_t * kernel)
{
  sanitizer_kernel_map_entry_t *e;
  e = (sanitizer_kernel_map_entry_t *)
    hpcrun_malloc(sizeof(sanitizer_kernel_map_entry_t));
  e->kernel = kernel;

  return e;
}


static sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_splay(sanitizer_kernel_map_entry_t *root, cct_node_t * key)
{
  REGULAR_SPLAY_TREE(sanitizer_kernel_map_entry_s, root, key, kernel, left, right);
  return root;
}


static void
sanitizer_kernel_map_delete_root()
{
  TMSG(DEFER_CTXT, "kernel %p: delete", sanitizer_kernel_map_root->kernel);

  if (sanitizer_kernel_map_root->left == NULL) {
    sanitizer_kernel_map_root = sanitizer_kernel_map_root->right;
  } else {
    sanitizer_kernel_map_root->left = 
      sanitizer_kernel_map_splay(sanitizer_kernel_map_root->left, 
			   sanitizer_kernel_map_root->kernel);
    sanitizer_kernel_map_root->left->right = sanitizer_kernel_map_root->right;
    sanitizer_kernel_map_root = sanitizer_kernel_map_root->left;
  }
}


static sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_init_internal(cct_node_t * kernel)
{
  sanitizer_kernel_map_entry_t *entry = NULL;

  if (sanitizer_kernel_map_root != NULL) {
    sanitizer_kernel_map_root = 
      sanitizer_kernel_map_splay(sanitizer_kernel_map_root, kernel);

    if (kernel < sanitizer_kernel_map_root->kernel) {
      entry = sanitizer_kernel_map_entry_new(kernel);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_kernel_map_root->left;
      entry->right = sanitizer_kernel_map_root;
      sanitizer_kernel_map_root->left = NULL;
      sanitizer_kernel_map_root = entry;
    } else if (kernel > sanitizer_kernel_map_root->kernel) {
      entry = sanitizer_kernel_map_entry_new(kernel);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_kernel_map_root;
      entry->right = sanitizer_kernel_map_root->right;
      sanitizer_kernel_map_root->right = NULL;
      sanitizer_kernel_map_root = entry;
    } else {
      // kernel already present
      entry = sanitizer_kernel_map_root;
    }
  } else {
    entry = sanitizer_kernel_map_entry_new(kernel);
    entry->left = entry->right = NULL;
    sanitizer_kernel_map_root = entry;
  }

  return entry;
}


static sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_lookup_internal(cct_node_t * kernel)
{
  sanitizer_kernel_map_entry_t *result = NULL;
  sanitizer_kernel_map_root = sanitizer_kernel_map_splay(sanitizer_kernel_map_root, kernel);
  if (sanitizer_kernel_map_root && sanitizer_kernel_map_root->kernel == kernel) {
    result = sanitizer_kernel_map_root;
  }

  TMSG(DEFER_CTXT, "kernel map lookup: kernel=0x%lx (record %p)", kernel, result);
  return result;
}

/******************************************************************************
 * interface operations
 *****************************************************************************/


sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_lookup(cct_node_t * kernel)
{
  sanitizer_kernel_map_entry_t *result = sanitizer_kernel_map_lookup_internal(kernel);

  return result;
}


sanitizer_kernel_map_entry_t *
sanitizer_kernel_map_init(cct_node_t * kernel)
{
  sanitizer_kernel_map_entry_t *result = sanitizer_kernel_map_init_internal(kernel);
  
  return result;
}


void
sanitizer_kernel_map_delete(cct_node_t * kernel)
{
  sanitizer_kernel_map_root = sanitizer_kernel_map_splay(sanitizer_kernel_map_root, kernel);
  
  if (sanitizer_kernel_map_root && sanitizer_kernel_map_root->kernel == kernel) {
    sanitizer_kernel_map_delete_root();
  }
}
