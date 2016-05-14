/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/ompt/ompt-region-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_region_map_entry_s {
  uint64_t region_id;
  uint64_t refcnt;
  cct_node_t *call_path;
  struct ompt_region_map_entry_s *left;
  struct ompt_region_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_region_map_entry_t *ompt_region_map_root = NULL;
static spinlock_t ompt_region_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_region_map_entry_t *
ompt_region_map_entry_new(uint64_t region_id, cct_node_t *call_path)
{
  ompt_region_map_entry_t *e;
  e = (ompt_region_map_entry_t *)hpcrun_malloc(sizeof(ompt_region_map_entry_t));
  e->region_id = region_id;
  e->refcnt = 0;
  e->call_path = call_path;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_region_map_entry_t *
ompt_region_map_splay(ompt_region_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_region_map_entry_s, root, key, region_id, left, right);
  return root;
}


static void
ompt_region_map_delete_root()
{
  TMSG(DEFER_CTXT, "region %d: delete", ompt_region_map_root->region_id);

  if (ompt_region_map_root->left == NULL) {
    ompt_region_map_root = ompt_region_map_root->right;
  } else {
    ompt_region_map_root->left = 
      ompt_region_map_splay(ompt_region_map_root->left, 
			   ompt_region_map_root->region_id);
    ompt_region_map_root->left->right = ompt_region_map_root->right;
    ompt_region_map_root = ompt_region_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_region_map_entry_t *
ompt_region_map_lookup(uint64_t id)
{
  ompt_region_map_entry_t *result = NULL;
  spinlock_lock(&ompt_region_map_lock);

  ompt_region_map_root = ompt_region_map_splay(ompt_region_map_root, id);
  if (ompt_region_map_root && ompt_region_map_root->region_id == id) {
    result = ompt_region_map_root;
  }

  spinlock_unlock(&ompt_region_map_lock);

  TMSG(DEFER_CTXT, "region map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_region_map_insert(uint64_t region_id, cct_node_t *call_path)
{
  ompt_region_map_entry_t *node = ompt_region_map_entry_new(region_id, call_path);

  TMSG(DEFER_CTXT, "region map insert: id=0x%lx (record %p)", region_id, node);

  node->left = node->right = NULL;

  spinlock_lock(&ompt_region_map_lock);

  if (ompt_region_map_root != NULL) {
    ompt_region_map_root = 
      ompt_region_map_splay(ompt_region_map_root, region_id);

    if (region_id < ompt_region_map_root->region_id) {
      node->left = ompt_region_map_root->left;
      node->right = ompt_region_map_root;
      ompt_region_map_root->left = NULL;
    } else if (region_id > ompt_region_map_root->region_id) {
      node->left = ompt_region_map_root;
      node->right = ompt_region_map_root->right;
      ompt_region_map_root->right = NULL;
    } else {
      // region_id already present: fatal error since a region_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_region_map_root = node;

  spinlock_unlock(&ompt_region_map_lock);
}


// return true if record found; false otherwise
bool
ompt_region_map_refcnt_update(uint64_t region_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "region map refcnt_update: id=0x%lx (update %d)", 
       region_id, val);

  spinlock_lock(&ompt_region_map_lock);
  ompt_region_map_root = ompt_region_map_splay(ompt_region_map_root, region_id);

  if (ompt_region_map_root && 
      ompt_region_map_root->region_id == region_id) {
    uint64_t old = ompt_region_map_root->refcnt;
    ompt_region_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "region map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 region_id, old, ompt_region_map_root->refcnt);
    if (ompt_region_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "region map refcnt_update: id=0x%lx (deleting)",
           region_id);
      ompt_region_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_region_map_lock);
  return result;
}


uint64_t 
ompt_region_map_entry_refcnt_get(ompt_region_map_entry_t *node) 
{
  return node->refcnt;
}


void 
ompt_region_map_entry_callpath_set(ompt_region_map_entry_t *node, 
				  cct_node_t *call_path)
{
  node->call_path = call_path;
}


cct_node_t *
ompt_region_map_entry_callpath_get(ompt_region_map_entry_t *node)
{
  return node->call_path;
}



/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_region_map_count_helper(ompt_region_map_entry_t *node) 
{
  if (node) {
     int left = ompt_region_map_count_helper(node->left);
     int right = ompt_region_map_count_helper(node->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_region_map_count() 
{
  return ompt_region_map_count_helper(ompt_region_map_root);
}

