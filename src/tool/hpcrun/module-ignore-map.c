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

#include "module-ignore-map.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct module_ignore_map_entry_s {
  load_module_t *module;
  uint64_t refcnt;
  struct module_ignore_map_entry_s *left;
  struct module_ignore_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static module_ignore_fn_entry_t *module_ignore_fn = NULL;
static module_ignore_map_entry_t *module_ignore_map_root = NULL;
static spinlock_t module_ignore_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static module_ignore_map_entry_t *
module_ignore_map_entry_new(load_module_t *module)
{
  module_ignore_map_entry_t *e;
  e = (module_ignore_map_entry_t *)hpcrun_malloc(sizeof(module_ignore_map_entry_t));
  e->module = module;
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static module_ignore_map_entry_t *
module_ignore_map_splay(module_ignore_map_entry_t *root, load_module_t *key)
{
  REGULAR_SPLAY_TREE(module_ignore_map_entry_s, root, key, module, left, right);
  return root;
}


static void
module_ignore_map_delete_root()
{
  TMSG(DEFER_CTXT, "module %d: delete", module_ignore_map_root->module);

  if (module_ignore_map_root->left == NULL) {
    module_ignore_map_root = module_ignore_map_root->right;
  } else {
    module_ignore_map_root->left = 
      module_ignore_map_splay(module_ignore_map_root->left, 
			   module_ignore_map_root->module);
    module_ignore_map_root->left->right = module_ignore_map_root->right;
    module_ignore_map_root = module_ignore_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

module_ignore_map_entry_t *
module_ignore_map_lookup(load_module_t *module)
{
  module_ignore_map_entry_t *result = NULL;
  spinlock_lock(&module_ignore_map_lock);

  module_ignore_map_root = module_ignore_map_splay(module_ignore_map_root, module);
  if (module_ignore_map_root && module_ignore_map_root->module == module) {
    result = module_ignore_map_root;
  }

  spinlock_unlock(&module_ignore_map_lock);

  TMSG(DEFER_CTXT, "module map lookup: module=0x%lx (record %p)", module, result);
  return result;
}


void
module_ignore_map_insert(load_module_t *module)
{
  spinlock_lock(&module_ignore_map_lock);

  if (module_ignore_map_root != NULL) {
    module_ignore_map_root = 
      module_ignore_map_splay(module_ignore_map_root, module);

    if (module < module_ignore_map_root->module) {
      module_ignore_map_entry_t *entry = module_ignore_map_entry_new(module);
      TMSG(DEFER_CTXT, "module map insert: module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->left = module_ignore_map_root->left;
      entry->right = module_ignore_map_root;
      module_ignore_map_root->left = NULL;
      module_ignore_map_root = entry;
    } else if (module > module_ignore_map_root->module) {
      module_ignore_map_entry_t *entry = module_ignore_map_entry_new(module);
      TMSG(DEFER_CTXT, "module map insert: module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->left = module_ignore_map_root;
      entry->right = module_ignore_map_root->right;
      module_ignore_map_root->right = NULL;
      module_ignore_map_root = entry;
    } else {
      // module already present
    }
  } else {
      module_ignore_map_entry_t *entry = module_ignore_map_entry_new(module);
      module_ignore_map_root = entry;
  }

  spinlock_unlock(&module_ignore_map_lock);
}


// return true if record found; false otherwise
bool
module_ignore_map_refcnt_update(load_module_t *module, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "module map refcnt_update: module=0x%lx (update %d)", 
       module, val);

  spinlock_lock(&module_ignore_map_lock);
  module_ignore_map_root = module_ignore_map_splay(module_ignore_map_root, module);

  if (module_ignore_map_root && 
      module_ignore_map_root->module == module) {
    uint64_t old = module_ignore_map_root->refcnt;
    module_ignore_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "module map refcnt_update: module=0x%lx (%ld --> %ld)", 
	    module, old, module_ignore_map_root->refcnt);
    if (module_ignore_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "module map refcnt_update: module=0x%lx (deleting)",
           module);
      module_ignore_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&module_ignore_map_lock);
  return result;
}


uint64_t 
module_ignore_map_entry_refcnt_get(module_ignore_map_entry_t *entry) 
{
  return entry->refcnt;
}


void
module_ignore_map_register(module_ignore_fn_entry_t *entry)
{
  entry->next = module_ignore_fn;
  module_ignore_fn = entry;
}


bool
module_ignore_map_ignore(load_module_t *module)
{
  module_ignore_fn_entry_t* fn = module_ignore_fn;
  while (fn != NULL) {
    if (fn->fn(module)) {
      return true;
    }
    fn = fn->next;
  }
  return false;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
module_ignore_map_count_helper(module_ignore_map_entry_t *entry) 
{
  if (entry) {
     int left = module_ignore_map_count_helper(entry->left);
     int right = module_ignore_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
module_ignore_map_count() 
{
  return module_ignore_map_count_helper(module_ignore_map_root);
}


