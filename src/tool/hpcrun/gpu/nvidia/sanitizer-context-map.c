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

#include "cuda-api.h"
#include "sanitizer-context-map.h"
#include "sanitizer-stream-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct sanitizer_context_map_entry_s {
  CUcontext context;
  CUstream priority_stream;
  sanitizer_stream_map_entry_t *streams;
  struct sanitizer_context_map_entry_s *left;
  struct sanitizer_context_map_entry_s *right;
}; 

/******************************************************************************
 * global data 
 *****************************************************************************/

static sanitizer_context_map_entry_t *sanitizer_context_map_root = NULL;
static spinlock_t sanitizer_context_map_lock = SPINLOCK_UNLOCKED;


/******************************************************************************
 * private operations
 *****************************************************************************/

static sanitizer_context_map_entry_t *
sanitizer_context_map_entry_new(CUcontext context)
{
  sanitizer_context_map_entry_t *e;
  e = (sanitizer_context_map_entry_t *)
    hpcrun_malloc(sizeof(sanitizer_context_map_entry_t));
  e->context = context;
  e->priority_stream = cuda_priority_stream_create(context);
  e->streams = NULL;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_splay(sanitizer_context_map_entry_t *root, CUcontext key)
{
  REGULAR_SPLAY_TREE(sanitizer_context_map_entry_s, root, key, context, left, right);
  return root;
}


static void
sanitizer_context_map_delete_root()
{
  TMSG(DEFER_CTXT, "context %p: delete", sanitizer_context_map_root->context);

  if (sanitizer_context_map_root->left == NULL) {
    sanitizer_context_map_root = sanitizer_context_map_root->right;
  } else {
    sanitizer_context_map_root->left = 
      sanitizer_context_map_splay(sanitizer_context_map_root->left, 
			   sanitizer_context_map_root->context);
    sanitizer_context_map_root->left->right = sanitizer_context_map_root->right;
    sanitizer_context_map_root = sanitizer_context_map_root->left;
  }
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_init_internal(CUcontext context)
{
  sanitizer_context_map_entry_t *entry = NULL;

  if (sanitizer_context_map_root != NULL) {
    sanitizer_context_map_root = 
      sanitizer_context_map_splay(sanitizer_context_map_root, context);

    if (context < sanitizer_context_map_root->context) {
      entry = sanitizer_context_map_entry_new(context);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_context_map_root->left;
      entry->right = sanitizer_context_map_root;
      sanitizer_context_map_root->left = NULL;
      sanitizer_context_map_root = entry;
    } else if (context > sanitizer_context_map_root->context) {
      entry = sanitizer_context_map_entry_new(context);
      entry->left = entry->right = NULL;
      entry->left = sanitizer_context_map_root;
      entry->right = sanitizer_context_map_root->right;
      sanitizer_context_map_root->right = NULL;
      sanitizer_context_map_root = entry;
    } else {
      // context already present
      entry = sanitizer_context_map_root;
    }
  } else {
    entry = sanitizer_context_map_entry_new(context);
    entry->left = entry->right = NULL;
    sanitizer_context_map_root = entry;
  }

  return entry;
}


static sanitizer_context_map_entry_t *
sanitizer_context_map_lookup_internal(CUcontext context)
{
  sanitizer_context_map_entry_t *result = NULL;
  sanitizer_context_map_root = sanitizer_context_map_splay(sanitizer_context_map_root, context);
  if (sanitizer_context_map_root && sanitizer_context_map_root->context == context) {
    result = sanitizer_context_map_root;
  }

  TMSG(DEFER_CTXT, "context map lookup: context=0x%lx (record %p)", context, result);
  return result;
}

/******************************************************************************
 * interface operations
 *****************************************************************************/


sanitizer_context_map_entry_t *
sanitizer_context_map_lookup(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_lookup_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);

  return result;
}


sanitizer_context_map_entry_t *
sanitizer_context_map_init(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_entry_t *result = sanitizer_context_map_init_internal(context);

  spinlock_unlock(&sanitizer_context_map_lock);
  
  return result;
}


void
sanitizer_context_map_delete(CUcontext context)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_context_map_root = sanitizer_context_map_splay(sanitizer_context_map_root, context);
  
  if (sanitizer_context_map_root && sanitizer_context_map_root->context == context) {
    sanitizer_context_map_delete_root();
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_insert(CUcontext context, CUstream stream)
{
  spinlock_lock(&sanitizer_context_map_lock);

  sanitizer_stream_map_insert(&sanitizer_context_map_root->streams, stream);

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_stream_lock
(
 CUcontext context,
 CUstream stream
)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  if ((entry = sanitizer_context_map_lookup_internal(context)) != NULL) {
    sanitizer_stream_map_stream_lock(&entry->streams, stream);
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


void
sanitizer_context_map_stream_unlock
(
 CUcontext context,
 CUstream stream
)
{
  sanitizer_context_map_entry_t *entry = NULL;

  spinlock_lock(&sanitizer_context_map_lock);

  if ((entry = sanitizer_context_map_lookup_internal(context)) != NULL) {
    sanitizer_stream_map_stream_unlock(&entry->streams, stream);
  }

  spinlock_unlock(&sanitizer_context_map_lock);
}


CUstream
sanitizer_context_map_entry_priority_stream_get
(
 sanitizer_context_map_entry_t *entry
)
{
  return entry->priority_stream;
}
