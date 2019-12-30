/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <cuda.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "sanitizer-stream-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct sanitizer_stream_map_entry_s {
  CUstream stream;
  spinlock_t lock;
  struct sanitizer_stream_map_entry_s *left;
  struct sanitizer_stream_map_entry_s *right;
}; 


/******************************************************************************
 * private operations
 *****************************************************************************/

static sanitizer_stream_map_entry_t *
sanitizer_stream_map_splay
(
 sanitizer_stream_map_entry_t *root,
 CUstream key
)
{
  REGULAR_SPLAY_TREE(sanitizer_stream_map_entry_s, root, key, stream, left, right);
  return root;
}


static void
sanitizer_stream_map_delete_root
(
 sanitizer_stream_map_entry_t **root
)
{
  if ((*root)->left == NULL) {
    *root = (*root)->right;
  } else {
    (*root)->left = sanitizer_stream_map_splay((*root)->left, (*root)->stream);
    (*root)->left->right = (*root)->right;
    *root = (*root)->left;
  }
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_stream_map_entry_t *
sanitizer_stream_map_lookup
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
)
{
  sanitizer_stream_map_entry_t *result = NULL;

  *root = sanitizer_stream_map_splay(*root, stream);
  if ((*root) && (*root)->stream == stream) {
    result = *root;
  }

  return result;
}

void
sanitizer_stream_map_insert
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
)
{
  sanitizer_stream_map_entry_t *entry = NULL;

  if (*root != NULL) {
    *root = sanitizer_stream_map_splay(*root, stream);

    if (stream < (*root)->stream) {
      entry = sanitizer_stream_map_entry_new(stream);
      entry->left = entry->right = NULL;
      entry->left = (*root)->left;
      entry->right = *root;
      (*root)->left = NULL;
      *root = entry;
    } else if (stream > (*root)->stream) {
      entry = sanitizer_stream_map_entry_new(stream);
      entry->left = entry->right = NULL;
      entry->left = *root;
      entry->right = (*root)->right;
      (*root)->right = NULL;
      *root = entry;
    } else {
      // stream already present
      entry = (*root);
    }
  } else {
    entry = sanitizer_stream_map_entry_new(stream);
    entry->left = entry->right = NULL;
    *root = entry;
  }
}


void
sanitizer_stream_map_delete
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
)
{
  *root = sanitizer_stream_map_splay(*root, stream);
  if (*root && (*root)->stream == stream) {
    sanitizer_stream_map_delete_root(root);
  }
}


void
sanitizer_stream_map_stream_lock
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
)
{
  sanitizer_stream_map_entry_t *entry = NULL;

  if ((entry = sanitizer_stream_map_lookup(root, stream)) != NULL) {
    spinlock_lock(&(entry->lock));
  }
}


void
sanitizer_stream_map_stream_unlock
(
 sanitizer_stream_map_entry_t **root,
 CUstream stream
)
{
  sanitizer_stream_map_entry_t *entry = NULL;

  if ((entry = sanitizer_stream_map_lookup(root, stream)) != NULL) {
    spinlock_unlock(&(entry->lock));
  }
}


sanitizer_stream_map_entry_t *
sanitizer_stream_map_entry_new(CUstream stream)
{
  sanitizer_stream_map_entry_t *e;
  e = (sanitizer_stream_map_entry_t *)
    hpcrun_malloc(sizeof(sanitizer_stream_map_entry_t));
  e->stream = stream;
  e->left = NULL;
  e->right = NULL;

  spinlock_init(&(e->lock));

  return e;
}
