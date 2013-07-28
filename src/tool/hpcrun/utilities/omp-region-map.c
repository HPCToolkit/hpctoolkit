/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/utilities/omp-region-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct omp_region_map_entry_s {
  uint64_t region_id;
  uint64_t refcnt;
  cct_node_t *call_path;
  struct omp_region_map_entry_s *left;
  struct omp_region_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static omp_region_map_entry_t *omp_region_map_root = NULL;
static spinlock_t omp_region_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static omp_region_map_entry_t *
new_omp_region_map_entry(uint64_t region_id, cct_node_t *call_path)
{
  omp_region_map_entry_t *e;
  e = (omp_region_map_entry_t *)hpcrun_malloc(sizeof(omp_region_map_entry_t));
  e->region_id = region_id;
  e->refcnt = 0;
  e->call_path = call_path;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static omp_region_map_entry_t *
omp_region_map_splay(omp_region_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(omp_region_map_entry_s, root, key, region_id, left, right);
  return root;
}


static void
omp_region_map_delete_root()
{
  TMSG(DEFER_CTXT, "region %d: delete", omp_region_map_root->region_id);

  if (omp_region_map_root->left == NULL) {
    omp_region_map_root = omp_region_map_root->right;
  } else {
    omp_region_map_root->left = 
      omp_region_map_splay(omp_region_map_root->left, 
			   omp_region_map_root->region_id);
    omp_region_map_root->left->right = omp_region_map_root->right;
    omp_region_map_root = omp_region_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

uint64_t 
omp_region_map_entry_refcnt_get(omp_region_map_entry_t *node) 
{
  return node->refcnt;
}


omp_region_map_entry_t *
omp_region_map_lookup(uint64_t id)
{
  omp_region_map_entry_t *result = NULL;
  spinlock_lock(&omp_region_map_lock);

  omp_region_map_root = omp_region_map_splay(omp_region_map_root, id);
  if (omp_region_map_root && omp_region_map_root->region_id == id) {
    result = omp_region_map_root;
  }

  spinlock_unlock(&omp_region_map_lock);
  return result;
}


void
omp_region_map_insert(uint64_t region_id, cct_node_t *call_path)
{
  omp_region_map_entry_t *node = new_omp_region_map_entry(region_id, call_path);

  node->left = node->right = NULL;

  spinlock_lock(&omp_region_map_lock);

  if (omp_region_map_root != NULL) {
    omp_region_map_root = 
      omp_region_map_splay(omp_region_map_root, region_id);

    if (region_id < omp_region_map_root->region_id) {
      node->left = omp_region_map_root->left;
      node->right = omp_region_map_root;
      omp_region_map_root->left = NULL;
    } else if (region_id > omp_region_map_root->region_id) {
      node->left = omp_region_map_root;
      node->right = omp_region_map_root->right;
      omp_region_map_root->right = NULL;
    } else {
      // region_id already present: fatal error since a region_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  omp_region_map_root = node;

  spinlock_unlock(&omp_region_map_lock);
}


// return true if record found; false otherwise
bool
omp_region_map_refcnt_update(uint64_t region_id, uint64_t val)
{
  bool result = false; 
  TMSG(DEFER_CTXT, "update region %d with %d", region_id, val);

  spinlock_lock(&omp_region_map_lock);
  omp_region_map_root = omp_region_map_splay(omp_region_map_root, region_id);

  if (omp_region_map_root && 
      omp_region_map_root->region_id == region_id) {
    omp_region_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "region %d: increased reference count by %d to %d", 
	 region_id, val, omp_region_map_root->refcnt);
    if (omp_region_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "I am here for delete");
      omp_region_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&omp_region_map_lock);
  return result;
}


void 
omp_region_map_entry_callpath_set(omp_region_map_entry_t *node, 
				  cct_node_t *call_path)
{
  node->call_path = call_path;
}


cct_node_t *
omp_region_map_entry_callpath_get(omp_region_map_entry_t *node)
{
  return node->call_path;
}
