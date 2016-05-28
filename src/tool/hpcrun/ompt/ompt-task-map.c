/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/ompt/ompt-task-map.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_task_map_entry_s {
  uint64_t task_id;
  cct_node_t *call_path;
  struct ompt_task_map_entry_s *left;
  struct ompt_task_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static ompt_task_map_entry_t *ompt_task_map_root = NULL;
static ompt_task_map_entry_t *freelist = NULL;

static spinlock_t ompt_task_map_lock = SPINLOCK_UNLOCKED;




/******************************************************************************
 * private operations
 *****************************************************************************/

ompt_task_map_entry_t *
ompt_task_map_entry_alloc()
{
  ompt_task_map_entry_t *e;
  if (freelist) {  
    e = freelist;
    freelist = freelist->left;
  } else {
    e = (ompt_task_map_entry_t *) hpcrun_malloc(sizeof(ompt_task_map_entry_t));
  }
  return e;
}


void
ompt_task_map_entry_free(ompt_task_map_entry_t *e)
{
  e->left = freelist;
  freelist = e;
}


static ompt_task_map_entry_t *
ompt_task_map_entry_new(uint64_t task_id, cct_node_t *call_path)
{
  ompt_task_map_entry_t *e;
  e = ompt_task_map_entry_alloc();
  e->task_id = task_id;
  e->call_path = call_path;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static ompt_task_map_entry_t *
ompt_task_map_splay(ompt_task_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(ompt_task_map_entry_s, root, key, task_id, left, right);
  return root;
}

#if 0
// FIXME -- this will be needed when the taskmap is populated and used
static void
ompt_task_map_delete_root()
{
  TMSG(DEFER_CTXT, "region %d: delete", ompt_task_map_root->task_id);

  if (ompt_task_map_root->left == NULL) {
    ompt_task_map_root = ompt_task_map_root->right;
  } else {
    ompt_task_map_root->left = 
      ompt_task_map_splay(ompt_task_map_root->left, 
			   ompt_task_map_root->task_id);
    ompt_task_map_root->left->right = ompt_task_map_root->right;
    ompt_task_map_root = ompt_task_map_root->left;
  }
}
#endif



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_task_map_entry_t *
ompt_task_map_lookup(uint64_t task_id)
{
  ompt_task_map_entry_t *result = NULL;
#if 0 // fixme task map disabled!
  spinlock_lock(&ompt_task_map_lock);

  ompt_task_map_root = ompt_task_map_splay(ompt_task_map_root, task_id);
  if (ompt_task_map_root && ompt_task_map_root->task_id == task_id) {
    result = ompt_task_map_root;
  }

  spinlock_unlock(&ompt_task_map_lock);
#endif
  return result;
}


void
ompt_task_map_insert(uint64_t task_id, cct_node_t *call_path)
{
  ompt_task_map_entry_t *node = ompt_task_map_entry_new(task_id, call_path);

  node->left = node->right = NULL;

  spinlock_lock(&ompt_task_map_lock);

  if (ompt_task_map_root != NULL) {
    ompt_task_map_root = 
      ompt_task_map_splay(ompt_task_map_root, task_id);

    if (task_id < ompt_task_map_root->task_id) {
      node->left = ompt_task_map_root->left;
      node->right = ompt_task_map_root;
      ompt_task_map_root->left = NULL;
    } else if (task_id > ompt_task_map_root->task_id) {
      node->left = ompt_task_map_root;
      node->right = ompt_task_map_root->right;
      ompt_task_map_root->right = NULL;
    } else {
      // task_id already present: fatal error since a task_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_task_map_root = node;

  spinlock_unlock(&ompt_task_map_lock);
}

void 
ompt_task_map_entry_callpath_set(ompt_task_map_entry_t *node, 
				  cct_node_t *call_path)
{
  node->call_path = call_path;
}


cct_node_t *
ompt_task_map_entry_callpath_get(ompt_task_map_entry_t *node)
{
  return node->call_path;
}
