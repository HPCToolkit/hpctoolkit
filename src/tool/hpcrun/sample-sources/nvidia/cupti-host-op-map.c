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

#include "cupti-host-op-map.h"
#include "cupti-activity-queue.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_host_op_map_entry_s {
  uint64_t host_op_id;
  uint64_t refcnt;
  uint64_t host_op_seq_id;
  cupti_activity_queue_entry_t **cupti_activity_queue;
  cct_node_t *target;
  struct cupti_host_op_map_entry_s *left;
  struct cupti_host_op_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_host_op_map_entry_t *cupti_host_op_map_root = NULL;
static spinlock_t cupti_host_op_map_lock = SPINLOCK_UNLOCKED;



/******************************************************************************
 * private operations
 *****************************************************************************/

static cupti_host_op_map_entry_t *
cupti_host_op_map_entry_new(uint64_t host_op_id,
                           uint64_t host_op_seq_id,
                           cct_node_t *target)
{
  cupti_host_op_map_entry_t *e;
  e = (cupti_host_op_map_entry_t *)hpcrun_malloc(sizeof(cupti_host_op_map_entry_t));
  e->host_op_id = host_op_id;
  e->refcnt = 0;
  e->host_op_seq_id = host_op_seq_id;
  e->target = target;
  e->left = NULL;
  e->right = NULL;
  e->cupti_activity_queue = cupti_activity_queue_head();

  return e;
}


static cupti_host_op_map_entry_t *
cupti_host_op_map_splay(cupti_host_op_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(cupti_host_op_map_entry_s, root, key, host_op_id, left, right);
  return root;
}


static void
cupti_host_op_map_delete_root()
{
  TMSG(DEFER_CTXT, "host op %d: delete", cupti_host_op_map_root->host_op_id);

  if (cupti_host_op_map_root->left == NULL) {
    cupti_host_op_map_root = cupti_host_op_map_root->right;
  } else {
    cupti_host_op_map_root->left = 
      cupti_host_op_map_splay(cupti_host_op_map_root->left, 
			   cupti_host_op_map_root->host_op_id);
    cupti_host_op_map_root->left->right = cupti_host_op_map_root->right;
    cupti_host_op_map_root = cupti_host_op_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_host_op_map_entry_t *
cupti_host_op_map_lookup(uint64_t id)
{
  cupti_host_op_map_entry_t *result = NULL;

  spinlock_lock(&cupti_host_op_map_lock);
  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, id);
  if (cupti_host_op_map_root && cupti_host_op_map_root->host_op_id == id) {
    result = cupti_host_op_map_root;
  }
  spinlock_unlock(&cupti_host_op_map_lock);

  TMSG(DEFER_CTXT, "host op map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cupti_host_op_map_insert(uint64_t host_op_id,
                        uint64_t host_op_seq_id,
                        cct_node_t *target)
{
  cupti_host_op_map_entry_t *entry = cupti_host_op_map_entry_new(host_op_id, host_op_seq_id, target);

  TMSG(DEFER_CTXT, "host op map insert: id=0x%lx seq_id=0x%lx", host_op_id, host_op_seq_id);

  entry->left = entry->right = NULL;

  spinlock_lock(&cupti_host_op_map_lock);

  if (cupti_host_op_map_root != NULL) {
    cupti_host_op_map_root = 
      cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

    if (host_op_id < cupti_host_op_map_root->host_op_id) {
      entry->left = cupti_host_op_map_root->left;
      entry->right = cupti_host_op_map_root;
      cupti_host_op_map_root->left = NULL;
    } else if (host_op_id > cupti_host_op_map_root->host_op_id) {
      entry->left = cupti_host_op_map_root;
      entry->right = cupti_host_op_map_root->right;
      cupti_host_op_map_root->right = NULL;
    } else {
      // host_op_id already present: fatal error since a host_op_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  cupti_host_op_map_root = entry;

  spinlock_unlock(&cupti_host_op_map_lock);
}


// return true if record found; false otherwise
bool
cupti_host_op_map_refcnt_update(uint64_t host_op_id, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (update %d)", 
       host_op_id, val);

  spinlock_lock(&cupti_host_op_map_lock);

  cupti_host_op_map_root = cupti_host_op_map_splay(cupti_host_op_map_root, host_op_id);

  if (cupti_host_op_map_root && 
      cupti_host_op_map_root->host_op_id == host_op_id) {
    uint64_t old = cupti_host_op_map_root->refcnt;
    cupti_host_op_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (%ld --> %ld)", 
	  host_op_id, old, cupti_host_op_map_root->refcnt);
    if (cupti_host_op_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "host op map refcnt_update: id=0x%lx (deleting)",
           host_op_id);
      cupti_host_op_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&cupti_host_op_map_lock);

  return result;
}


cct_node_t *
cupti_host_op_map_entry_target_get(cupti_host_op_map_entry_t *entry)
{
  return entry->target;
}


uint64_t
cupti_host_op_map_entry_seq_id_get(cupti_host_op_map_entry_t *entry)
{
  return entry->host_op_seq_id;
}

cupti_activity_queue_entry_t **
cupti_host_op_map_entry_activity_queue_get(cupti_host_op_map_entry_t *entry)
{
  return entry->cupti_activity_queue;
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_host_op_map_count_helper(cupti_host_op_map_entry_t *entry) 
{
  if (entry) {
     int left = cupti_host_op_map_count_helper(entry->left);
     int right = cupti_host_op_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_host_op_map_count() 
{
  return cupti_host_op_map_count_helper(cupti_host_op_map_root);
}


