/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/ompt/ompt-function-id-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_function_id_map_entry_s {
  uint64_t function_id;  // cupti function id
  uint64_t refcnt;
  uint64_t function_index;
  uint64_t cubin_id;
  struct ompt_function_id_map_entry_s *left;
  struct ompt_function_id_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_function_id_map_entry_t *ompt_function_id_map_root = NULL;
static spinlock_t ompt_function_id_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_function_id_map_entry_t *
ompt_function_id_map_entry_new(uint64_t function_id, uint64_t function_index, uint64_t cubin_id)
{
  ompt_function_id_map_entry_t *e;
  e = (ompt_function_id_map_entry_t *)hpcrun_malloc(sizeof(ompt_function_id_map_entry_t));
  e->function_id = function_id;
  e->function_index = function_index;
  e->cubin_id = cubin_id;
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_function_id_map_entry_t *
ompt_function_id_map_splay(ompt_function_id_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_function_id_map_entry_s, root, key, function_id, left, right);
  return root;
}


static void
ompt_function_id_map_delete_root()
{
  TMSG(DEFER_CTXT, "function_id %d: delete", ompt_function_id_map_root->function_id);

  if (ompt_function_id_map_root->left == NULL) {
    ompt_function_id_map_root = ompt_function_id_map_root->right;
  } else {
    ompt_function_id_map_root->left = 
      ompt_function_id_map_splay(ompt_function_id_map_root->left, 
			   ompt_function_id_map_root->function_id);
    ompt_function_id_map_root->left->right = ompt_function_id_map_root->right;
    ompt_function_id_map_root = ompt_function_id_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_function_id_map_entry_t *
ompt_function_id_map_lookup(uint64_t id)
{
  ompt_function_id_map_entry_t *result = NULL;
  spinlock_lock(&ompt_function_id_map_lock);

  ompt_function_id_map_root = ompt_function_id_map_splay(ompt_function_id_map_root, id);
  if (ompt_function_id_map_root && ompt_function_id_map_root->function_id == id) {
    result = ompt_function_id_map_root;
  }

  spinlock_unlock(&ompt_function_id_map_lock);

  TMSG(DEFER_CTXT, "function_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_function_id_map_insert(uint64_t function_id, uint64_t function_index, uint64_t cubin_id)
{
  ompt_function_id_map_entry_t *entry = ompt_function_id_map_entry_new(function_id, function_index, cubin_id);

  TMSG(DEFER_CTXT, "function_id map insert: id=0x%lx (record %p)", function_id, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_function_id_map_lock);

  if (ompt_function_id_map_root != NULL) {
    ompt_function_id_map_root = 
      ompt_function_id_map_splay(ompt_function_id_map_root, function_id);

    if (function_id < ompt_function_id_map_root->function_id) {
      entry->left = ompt_function_id_map_root->left;
      entry->right = ompt_function_id_map_root;
      ompt_function_id_map_root->left = NULL;
    } else if (function_id > ompt_function_id_map_root->function_id) {
      entry->left = ompt_function_id_map_root;
      entry->right = ompt_function_id_map_root->right;
      ompt_function_id_map_root->right = NULL;
    } else {
      // function_id already present: fatal error since a function_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_function_id_map_root = entry;

  spinlock_unlock(&ompt_function_id_map_lock);
}


// return true if record found; false otherwise
bool
ompt_function_id_map_refcnt_update(uint64_t function_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "function_id map refcnt_update: id=0x%lx (update %d)", 
       function_id, val);

  spinlock_lock(&ompt_function_id_map_lock);
  ompt_function_id_map_root = ompt_function_id_map_splay(ompt_function_id_map_root, function_id);

  if (ompt_function_id_map_root && 
      ompt_function_id_map_root->function_id == function_id) {
    uint64_t old = ompt_function_id_map_root->refcnt;
    ompt_function_id_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "function_id map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 function_id, old, ompt_function_id_map_root->refcnt);
    if (ompt_function_id_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "function_id map refcnt_update: id=0x%lx (deleting)",
           function_id);
      ompt_function_id_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_function_id_map_lock);
  return result;
}


uint64_t 
ompt_function_id_map_entry_refcnt_get(ompt_function_id_map_entry_t *entry) 
{
  return entry->refcnt;
}


uint64_t 
ompt_function_id_map_entry_function_index_get(ompt_function_id_map_entry_t *entry) 
{
  return entry->function_index;
}


uint64_t 
ompt_function_id_map_entry_cubin_id_get(ompt_function_id_map_entry_t *entry) 
{
  return entry->cubin_id;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_function_id_map_count_helper(ompt_function_id_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_function_id_map_count_helper(entry->left);
     int right = ompt_function_id_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_function_id_map_count() 
{
  return ompt_function_id_map_count_helper(ompt_function_id_map_root);
}

