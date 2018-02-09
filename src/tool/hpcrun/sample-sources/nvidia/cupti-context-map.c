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

#include "cupti-context-map.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct cupti_context_map_entry_s {
  CUcontext context;  // cupti function id
  CUpti_ActivityKind *inserted_activities;
  CUpti_ActivityKind *enabled_activities;
  uint64_t refcnt;
  struct cupti_context_map_entry_s *left;
  struct cupti_context_map_entry_s *right;
}; 



/******************************************************************************
 * global data 
 *****************************************************************************/

static cupti_context_map_entry_t *cupti_context_map_root = NULL;
static spinlock_t cupti_context_map_lock = SPINLOCK_UNLOCKED;
static spinlock_t cupti_context_map_activity_lock = SPINLOCK_UNLOCKED;


size_t
cupti_context_map_activity_num()
{
  static size_t activity_num = 0;
  spinlock_lock(&cupti_context_map_activity_lock);
  if (!activity_num) {
    size_t i = 0;
    for (; i < CUPTI_ACTIVITY_KIND_FORCE_INT; ++i) {
      activity_num = i + 1;
    }
  }
  spinlock_unlock(&cupti_context_map_activity_lock);
  return activity_num;
}


/******************************************************************************
 * private operations
 *****************************************************************************/


static cupti_context_map_entry_t *
cupti_context_map_entry_new(CUcontext context)
{
  cupti_context_map_entry_t *e;
  e = (cupti_context_map_entry_t *)hpcrun_malloc(sizeof(cupti_context_map_entry_t));
  e->context = context;
  e->inserted_activities = (CUpti_ActivityKind *)hpcrun_malloc(sizeof(CUpti_ActivityKind) * cupti_activity_num());
  e->enabled_activities = (CUpti_ActivityKind *)hpcrun_malloc(sizeof(CUpti_ActivityKind) * cupti_activity_num());
  e->refcnt = 0;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cupti_context_map_entry_t *
cupti_context_map_splay(cupti_context_map_entry_t *root, CUcontext key)
{
  REGULAR_SPLAY_TREE(cupti_context_map_entry_s, root, key, context, left, right);
  return root;
}


static void
cupti_context_map_delete_root()
{
  TMSG(DEFER_CTXT, "context %d: delete", cupti_context_map_root->context);

  if (cupti_context_map_root->left == NULL) {
    cupti_context_map_root = cupti_context_map_root->right;
  } else {
    cupti_context_map_root->left = 
      cupti_context_map_splay(cupti_context_map_root->left, 
			   cupti_context_map_root->context);
    cupti_context_map_root->left->right = cupti_context_map_root->right;
    cupti_context_map_root = cupti_context_map_root->left;
  }
}



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_context_map_entry_t *
cupti_context_map_lookup(CUcontext context)
{
  cupti_context_map_entry_t *result = NULL;
  spinlock_lock(&cupti_context_map_lock);

  cupti_context_map_root = cupti_context_map_splay(cupti_context_map_root, context);
  if (cupti_context_map_root && cupti_context_map_root->context == context) {
    result = cupti_context_map_root;
  }

  spinlock_unlock(&cupti_context_map_lock);

  return result;
}


void
cupti_context_map_enable(CUcontext context, CUpti_ActivityKind activity)
{
  cupti_context_map_entry_t *entry = cupti_context_map_entry_new(context);

  TMSG(DEFER_CTXT, "context map insert: id=0x%lx (record %p)", context, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&cupti_context_map_lock);

  if (cupti_context_map_root != NULL) {
    cupti_context_map_root = 
      cupti_context_map_splay(cupti_context_map_root, context);

    if (context < cupti_context_map_root->context) {
      entry->left = cupti_context_map_root->left;
      entry->right = cupti_context_map_root;
      cupti_context_map_root->left = NULL;
    } else if (context > cupti_context_map_root->context) {
      entry->left = cupti_context_map_root;
      entry->right = cupti_context_map_root->right;
      cupti_context_map_root->right = NULL;
    } else {
      // context already present: fatal error since a context 
      //   should only be inserted once 
    }
  }
  CUpti_ActivityKind *inserted_vector = entry->inserted_activities;
  CUpti_ActivityKind *enabled_vector = entry->enabled_activities;
  inserted_vector[activity] = true;
  enabled_vector[activity] = true;
  cupti_context_map_root = entry;

  spinlock_unlock(&cupti_context_map_lock);
}


void
cupti_context_map_disable(CUcontext context, CUpti_ActivityKind activity)
{
  cupti_context_map_entry_t *entry = cupti_context_map_entry_new(context);

  TMSG(DEFER_CTXT, "context map insert: id=0x%lx (record %p)", context, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&cupti_context_map_lock);

  if (cupti_context_map_root != NULL) {
    cupti_context_map_root = 
      cupti_context_map_splay(cupti_context_map_root, context);

    if (context < cupti_context_map_root->context) {
      entry->left = cupti_context_map_root->left;
      entry->right = cupti_context_map_root;
      cupti_context_map_root->left = NULL;
    } else if (context > cupti_context_map_root->context) {
      entry->left = cupti_context_map_root;
      entry->right = cupti_context_map_root->right;
      cupti_context_map_root->right = NULL;
    } else {
      // context already present: fatal error since a context 
      //   should only be inserted once 
    }
  }
  CUpti_ActivityKind *enabled_vector = entry->enabled_activities;
  enabled_vector[activity] = false;
  cupti_context_map_root = entry;

  spinlock_unlock(&cupti_context_map_lock);
}


// return true if record found; false otherwise
bool
cupti_context_map_refcnt_update(CUcontext context, int val)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "context map refcnt_update: id=0x%lx (update %d)", 
       context, val);

  spinlock_lock(&cupti_context_map_lock);
  cupti_context_map_root = cupti_context_map_splay(cupti_context_map_root, context);

  if (cupti_context_map_root && 
      cupti_context_map_root->context == context) {
    uint64_t old = cupti_context_map_root->refcnt;
    cupti_context_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "context map refcnt_update: id=0x%lx (%ld --> %ld)", 
	 context, old, cupti_context_map_root->refcnt);
    if (cupti_context_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "context map refcnt_update: id=0x%lx (deleting)",
           context);
      cupti_context_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&cupti_context_map_lock);
  return result;
}


uint64_t 
cupti_context_map_entry_refcnt_get(cupti_context_map_entry_t *entry) 
{
  return entry->refcnt;
}


bool 
cupti_context_map_entry_activity_get(cupti_context_map_entry_t *entry, CUpti_ActivityKind activity) 
{
  CUpti_ActivityKind *inserted_activities = entry->inserted_activities;
  return inserted_activities[activity];
}


bool 
cupti_context_map_entry_activity_status_get(cupti_context_map_entry_t *entry, CUpti_ActivityKind activity) 
{
  CUpti_ActivityKind *enabled_activities = entry->enabled_activities;
  return enabled_activities[activity];
}

/******************************************************************************
 * debugging code
 *****************************************************************************/

static int 
cupti_context_map_count_helper(cupti_context_map_entry_t *entry) 
{
  if (entry) {
     int left = cupti_context_map_count_helper(entry->left);
     int right = cupti_context_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
cupti_context_map_count() 
{
  return cupti_context_map_count_helper(cupti_context_map_root);
}


