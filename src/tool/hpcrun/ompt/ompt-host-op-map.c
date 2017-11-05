/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/ompt/ompt-host-op-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_host_op_map_entry_s {
  uint64_t host_op_id;
  uint64_t refcnt;
  cct_node_t *call_path;
  struct ompt_host_op_map_entry_s *left;
  struct ompt_host_op_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_host_op_map_entry_t *ompt_host_op_map_root = NULL;
static spinlock_t ompt_host_op_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static ompt_host_op_map_entry_t *
ompt_host_op_map_entry_new(uint64_t host_op_id, cct_node_t *call_path)
{
  ompt_host_op_map_entry_t *e;
  e = (ompt_host_op_map_entry_t *)hpcrun_malloc(sizeof(ompt_host_op_map_entry_t));
  e->host_op_id = host_op_id;
  e->refcnt = 0;
  e->call_path = call_path;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_host_op_map_entry_t *
ompt_host_op_map_splay(ompt_host_op_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_host_op_map_entry_s, root, key, host_op_id, left, right);
  return root;
}


static void
ompt_host_op_map_delete_root()
{
  TMSG(DEFER_CTXT, "host op %d: delete", ompt_host_op_map_root->host_op_id);

  if (ompt_host_op_map_root->left == NULL) {
    ompt_host_op_map_root = ompt_host_op_map_root->right;
  } else {
    ompt_host_op_map_root->left = 
      ompt_host_op_map_splay(ompt_host_op_map_root->left, 
			   ompt_host_op_map_root->host_op_id);
    ompt_host_op_map_root->left->right = ompt_host_op_map_root->right;
    ompt_host_op_map_root = ompt_host_op_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_host_op_map_entry_t *
ompt_host_op_map_lookup(uint64_t id)
{
  ompt_host_op_map_entry_t *result = NULL;
  spinlock_lock(&ompt_host_op_map_lock);

  ompt_host_op_map_root = ompt_host_op_map_splay(ompt_host_op_map_root, id);
  if (ompt_host_op_map_root && ompt_host_op_map_root->host_op_id == id) {
    result = ompt_host_op_map_root;
  }

  spinlock_unlock(&ompt_host_op_map_lock);

  TMSG(DEFER_CTXT, "host op map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_host_op_map_insert(uint64_t host_op_id, cct_node_t *call_path)
{
  ompt_host_op_map_entry_t *node = ompt_host_op_map_entry_new(host_op_id, call_path);

  TMSG(DEFER_CTXT, "host op map insert: id=0x%lx (record %p)", host_op_id, node);

  node->left = node->right = NULL;

  spinlock_lock(&ompt_host_op_map_lock);

  if (ompt_host_op_map_root != NULL) {
    ompt_host_op_map_root = 
      ompt_host_op_map_splay(ompt_host_op_map_root, host_op_id);

    if (host_op_id < ompt_host_op_map_root->host_op_id) {
      node->left = ompt_host_op_map_root->left;
      node->right = ompt_host_op_map_root;
      ompt_host_op_map_root->left = NULL;
    } else if (host_op_id > ompt_host_op_map_root->host_op_id) {
      node->left = ompt_host_op_map_root;
      node->right = ompt_host_op_map_root->right;
      ompt_host_op_map_root->right = NULL;
    } else {
      // host_op_id already present: fatal error since a host_op_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_host_op_map_root = node;

  spinlock_unlock(&ompt_host_op_map_lock);
}


// return true if record found; false otherwise
bool
ompt_host_op_map_refcnt_update(uint64_t host_op_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (update %d)", 
       host_op_id, val);

  spinlock_lock(&ompt_host_op_map_lock);
  ompt_host_op_map_root = ompt_host_op_map_splay(ompt_host_op_map_root, host_op_id);

  if (ompt_host_op_map_root && 
      ompt_host_op_map_root->host_op_id == host_op_id) {
    uint64_t old = ompt_host_op_map_root->refcnt;
    ompt_host_op_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 host_op_id, old, ompt_host_op_map_root->refcnt);
    if (ompt_host_op_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (deleting)",
           host_op_id);
      ompt_host_op_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_host_op_map_lock);
  return result;
}


uint64_t 
ompt_host_op_map_entry_refcnt_get(ompt_host_op_map_entry_t *node) 
{
  return node->refcnt;
}


void 
ompt_host_op_map_entry_callpath_set(ompt_host_op_map_entry_t *node, 
				  cct_node_t *call_path)
{
  node->call_path = call_path;
}


cct_node_t *
ompt_host_op_map_entry_callpath_get(ompt_host_op_map_entry_t *node)
{
  return node->call_path;
}



/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_host_op_map_count_helper(ompt_host_op_map_entry_t *node) 
{
  if (node) {
     int left = ompt_host_op_map_count_helper(node->left);
     int right = ompt_host_op_map_count_helper(node->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_host_op_map_count() 
{
  return ompt_host_op_map_count_helper(ompt_host_op_map_root);
}


