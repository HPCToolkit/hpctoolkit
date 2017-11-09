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
  uint64_t host_op_seq_id;
  ompt_region_map_entry_t *region_entry;
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
ompt_host_op_map_entry_new(uint64_t host_op_id,
                           uint64_t host_op_seq_id,
                           ompt_region_map_entry_t *map_entry)
{
  ompt_host_op_map_entry_t *e;
  e = (ompt_host_op_map_entry_t *)hpcrun_malloc(sizeof(ompt_host_op_map_entry_t));
  e->host_op_id = host_op_id;
  e->refcnt = 0;
  e->host_op_seq_id = host_op_seq_id;
  e->region_entry = map_entry;
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
ompt_host_op_map_insert(uint64_t host_op_id,
                        uint64_t host_op_seq_id,
                        ompt_region_map_entry_t *map_entry)
{
  ompt_host_op_map_entry_t *entry = ompt_host_op_map_entry_new(host_op_id, host_op_seq_id, map_entry);

  TMSG(DEFER_CTXT, "host op map insert: id=0x%lx seq_id=0x%lx", host_op_id, host_op_seq_id);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_host_op_map_lock);

  if (ompt_host_op_map_root != NULL) {
    ompt_host_op_map_root = 
      ompt_host_op_map_splay(ompt_host_op_map_root, host_op_id);

    if (host_op_id < ompt_host_op_map_root->host_op_id) {
      entry->left = ompt_host_op_map_root->left;
      entry->right = ompt_host_op_map_root;
      ompt_host_op_map_root->left = NULL;
    } else if (host_op_id > ompt_host_op_map_root->host_op_id) {
      entry->left = ompt_host_op_map_root;
      entry->right = ompt_host_op_map_root->right;
      ompt_host_op_map_root->right = NULL;
    } else {
      // host_op_id already present: fatal error since a host_op_id 
      //   should only be inserted once 
      assert(0);
    }
  }

  spinlock_unlock(&ompt_host_op_map_lock);
  ompt_host_op_map_root = entry;
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


ompt_region_map_entry_t *
ompt_host_op_map_entry_region_map_entry_get(ompt_host_op_map_entry_t *entry)
{
  return entry->map_entry;
}


uint64_t
ompt_host_op_map_entry_seq_id_get(ompt_host_op_map_entry_t *entry)
{
  return entry->host_op_seq_id;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
ompt_host_op_map_count_helper(ompt_host_op_map_entry_t *entry) 
{
  if (entry) {
     int left = ompt_host_op_map_count_helper(entry->left);
     int right = ompt_host_op_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_host_op_map_count() 
{
  return ompt_host_op_map_count_helper(ompt_host_op_map_root);
}


