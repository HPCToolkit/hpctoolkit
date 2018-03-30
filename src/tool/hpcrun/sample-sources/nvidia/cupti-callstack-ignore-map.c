/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <dlfcn.h>  // dlopen

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-activity-api.h"
#include "cupti-callstack-ignore-map.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_callstack_ignore_map_entry_s {
  load_module_t *module;
  uint64_t refcnt;
  struct cupti_callstack_ignore_map_entry_s *left;
  struct cupti_callstack_ignore_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_callstack_ignore_map_entry_t *cupti_callstack_ignore_map_root = NULL;
static spinlock_t cupti_callstack_ignore_map_lock = SPINLOCK_UNLOCKED;

/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_callstack_ignore_map_entry_t *
cupti_callstack_ignore_map_entry_new(load_module_t *module)
{
  cupti_callstack_ignore_map_entry_t *e;
  e = (cupti_callstack_ignore_map_entry_t *)hpcrun_malloc(sizeof(cupti_callstack_ignore_map_entry_t));
  e->module = module;
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_callstack_ignore_map_entry_t *
cupti_callstack_ignore_map_splay(cupti_callstack_ignore_map_entry_t *root, load_module_t *key)
{
  REGULAR_SPLAY_TREE(cupti_callstack_ignore_map_entry_s, root, key, module, left, right);
  return root;
}


static void
cupti_callstack_ignore_map_delete_root()
{
  TMSG(DEFER_CTXT, "module %d: delete", cupti_callstack_ignore_map_root->module);

  if (cupti_callstack_ignore_map_root->left == NULL) {
    cupti_callstack_ignore_map_root = cupti_callstack_ignore_map_root->right;
  } else {
    cupti_callstack_ignore_map_root->left = 
      cupti_callstack_ignore_map_splay(cupti_callstack_ignore_map_root->left, 
			   cupti_callstack_ignore_map_root->module);
    cupti_callstack_ignore_map_root->left->right = cupti_callstack_ignore_map_root->right;
    cupti_callstack_ignore_map_root = cupti_callstack_ignore_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_callstack_ignore_map_entry_t *
cupti_callstack_ignore_map_lookup(load_module_t *module)
{
  cupti_callstack_ignore_map_entry_t *result = NULL;
  spinlock_lock(&cupti_callstack_ignore_map_lock);

  cupti_callstack_ignore_map_root = cupti_callstack_ignore_map_splay(cupti_callstack_ignore_map_root, module);
  if (cupti_callstack_ignore_map_root && cupti_callstack_ignore_map_root->module == module) {
    result = cupti_callstack_ignore_map_root;
  }

  spinlock_unlock(&cupti_callstack_ignore_map_lock);

  TMSG(DEFER_CTXT, "module map lookup: module=0x%lx (record %p)", module, result);
  return result;
}


void
cupti_callstack_ignore_map_insert(load_module_t *module)
{
  spinlock_lock(&cupti_callstack_ignore_map_lock);

  if (cupti_callstack_ignore_map_root != NULL) {
    cupti_callstack_ignore_map_root = 
      cupti_callstack_ignore_map_splay(cupti_callstack_ignore_map_root, module);

    if (module < cupti_callstack_ignore_map_root->module) {
      cupti_callstack_ignore_map_entry_t *entry = cupti_callstack_ignore_map_entry_new(module);
      TMSG(DEFER_CTXT, "module map insert:module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->left = cupti_callstack_ignore_map_root->left;
      entry->right = cupti_callstack_ignore_map_root;
      cupti_callstack_ignore_map_root->left = NULL;
      cupti_callstack_ignore_map_root = entry;
    } else if (module > cupti_callstack_ignore_map_root->module) {
      cupti_callstack_ignore_map_entry_t *entry = cupti_callstack_ignore_map_entry_new(module);
      TMSG(DEFER_CTXT, "module map insert:module=0x%lx (record %p)", module, entry);
      entry->left = entry->right = NULL;
      entry->left = cupti_callstack_ignore_map_root;
      entry->right = cupti_callstack_ignore_map_root->right;
      cupti_callstack_ignore_map_root->right = NULL;
      cupti_callstack_ignore_map_root = entry;
    } else {
      // module already present
    }
  } else {
      cupti_callstack_ignore_map_entry_t *entry = cupti_callstack_ignore_map_entry_new(module);
      cupti_callstack_ignore_map_root = entry;
  }

  spinlock_unlock(&cupti_callstack_ignore_map_lock);
}


// return true if record found; false otherwise
bool
cupti_callstack_ignore_map_refcnt_update(load_module_t *module, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "module map refcnt_update:module=0x%lx (update %d)", 
       module, val);

  spinlock_lock(&cupti_callstack_ignore_map_lock);
  cupti_callstack_ignore_map_root = cupti_callstack_ignore_map_splay(cupti_callstack_ignore_map_root, module);

  if (cupti_callstack_ignore_map_root && 
      cupti_callstack_ignore_map_root->module == module) {
    uint64_t old = cupti_callstack_ignore_map_root->refcnt;
    cupti_callstack_ignore_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "module map refcnt_update:module=0x%lx (%ld --> %ld)", 
	    module, old, cupti_callstack_ignore_map_root->refcnt);
    if (cupti_callstack_ignore_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "module map refcnt_update: module=0x%lx (deleting)",
           module);
      cupti_callstack_ignore_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&cupti_callstack_ignore_map_lock);
  return result;
}


uint64_t 
cupti_callstack_ignore_map_entry_refcnt_get(cupti_callstack_ignore_map_entry_t *entry) 
{
  return entry->refcnt;
}


bool
cupti_callstack_ignore_map_ignore(load_module_t *module)
{
  if (cupti_lm_contains_fn(module->name, "cudaLaunchKernel") ||
      cupti_lm_contains_fn(module->name, "cuLaunchKernel") ||
      cupti_lm_contains_fn(module->name, "cuptiActivityEnable")) {
    return true;
  }
  return false;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_callstack_ignore_map_count_helper(cupti_callstack_ignore_map_entry_t *entry) 
{
  if (entry) {
     int left = cupti_callstack_ignore_map_count_helper(entry->left);
     int right = cupti_callstack_ignore_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_callstack_ignore_map_count() 
{
  return cupti_callstack_ignore_map_count_helper(cupti_callstack_ignore_map_root);
}



