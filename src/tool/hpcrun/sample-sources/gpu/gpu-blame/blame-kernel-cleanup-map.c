//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "blame-kernel-cleanup-map.h"         // kernel_node_t, queue_node_t

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <hpcrun/memory/hpcrun-malloc.h>      // hpcrun_malloc_safe
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define kernel_insert \
  typed_splay_insert(kernel_node)

#define kernel_lookup \
  typed_splay_lookup(kernel_node)

#define kernel_delete \
  typed_splay_delete(kernel_node)

#define kernel_forall \
  typed_splay_forall(kernel_node)

#define kernel_count \
  typed_splay_count(kernel_node)

#define kernel_alloc(free_list) \
  typed_splay_alloc(free_list, kernel_cleanup_map_entry_t)

#define kernel_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(kernel_node) kernel_cleanup_map_entry_t

typedef struct typed_splay_node(kernel_node) {
  struct typed_splay_node(kernel_node) *left;
  struct typed_splay_node(kernel_node) *right;
  uint64_t kernel_id; // key

  kernel_cleanup_data_t *data;
} typed_splay_node(kernel_node);

typed_splay_impl(kernel_node);



//******************************************************************************
// local data
//******************************************************************************

static kernel_cleanup_map_entry_t *kernel_cleanup_map_root = NULL;
static kernel_cleanup_map_entry_t *kernel_cleanup_map_free_list = NULL;
static kernel_cleanup_data_t *kcd_free_list = NULL;

static spinlock_t kernel_cleanup_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static kernel_cleanup_map_entry_t *
kernel_node_alloc()
{
  return kernel_alloc(&kernel_cleanup_map_free_list);
}


static kernel_cleanup_map_entry_t *
kernel_node_new
(
 uint64_t kernel_id,
 kernel_cleanup_data_t *data
)
{
  kernel_cleanup_map_entry_t *e = kernel_node_alloc();
  e->kernel_id = kernel_id;
  e->data = data;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

kernel_cleanup_map_entry_t*
kernel_cleanup_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_cleanup_map_lock);
  kernel_cleanup_map_entry_t *result = kernel_lookup(&kernel_cleanup_map_root, kernel_id);
  spinlock_unlock(&kernel_cleanup_map_lock);
  return result;
}


void
kernel_cleanup_map_insert
(
 uint64_t kernel_id,
 kernel_cleanup_data_t *data
)
{
  if (kernel_lookup(&kernel_cleanup_map_root, kernel_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&kernel_cleanup_map_lock);
    kernel_cleanup_map_entry_t *entry = kernel_node_new(kernel_id, data);
    kernel_insert(&kernel_cleanup_map_root, entry);
    spinlock_unlock(&kernel_cleanup_map_lock);
  }
}


void
kernel_cleanup_map_delete
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_cleanup_map_lock);
  kernel_cleanup_map_entry_t *node = kernel_delete(&kernel_cleanup_map_root, kernel_id);
  if (node) {
    kernel_free(&kernel_cleanup_map_free_list, node);
  }
  spinlock_unlock(&kernel_cleanup_map_lock);
}


kernel_cleanup_data_t*
kernel_cleanup_map_entry_data_get
(
 kernel_cleanup_map_entry_t *entry
)
{
  return entry->data;
}


kernel_cleanup_data_t*
kcd_alloc_helper
(
 void
)
{
  kernel_cleanup_data_t *first = kcd_free_list;

  if (first) {
    kcd_free_list = first->next;
  } else {
    first = (kernel_cleanup_data_t *) hpcrun_malloc_safe(sizeof(kernel_cleanup_data_t));
  }

  memset(first, 0, sizeof(kernel_cleanup_data_t));
  return first;
}


void
kcd_free_helper
(
 kernel_cleanup_data_t *node
)
{
  node->next = kcd_free_list;
  kcd_free_list = node;
}

