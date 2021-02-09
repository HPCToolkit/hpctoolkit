//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "opencl-event-map.h"		// event_node_t, queue_node_t

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define ak_insert \
  typed_splay_insert(ak_node)

#define ak_lookup \
  typed_splay_lookup(ak_node)

#define ak_delete \
  typed_splay_delete(ak_node)

#define ak_forall \
  typed_splay_forall(ak_node)

#define ak_count \
  typed_splay_count(ak_node)

#define ak_alloc(free_list) \
  typed_splay_alloc(free_list, active_kernels_entry_t)

#define ak_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(ak_node) active_kernels_entry_t

typedef struct typed_splay_node(ak_node) {
  struct typed_splay_node(ak_node) *left;
  struct typed_splay_node(ak_node) *right;
  
  uint64_t kernel_event_address; // key
  event_node_t *event_node;

} typed_splay_node(ak_node);

typed_splay_impl(ak_node);



//******************************************************************************
// local data
//******************************************************************************

static __thread active_kernels_entry_t *ak_map_root = NULL;
static __thread active_kernels_entry_t *ak_map_free_list = NULL;

static __thread spinlock_t ak_map_lock = SPINLOCK_UNLOCKED;
static long __thread size = 0;	// this should be unique for every ak splay-tree



//******************************************************************************
// private operations
//******************************************************************************

static active_kernels_entry_t *
ak_node_alloc()
{
  return ak_alloc(&ak_map_free_list);
}


static active_kernels_entry_t *
ak_node_new
(
 uint64_t ak_id,
 event_node_t *event_node
)
{
  active_kernels_entry_t *e = ak_node_alloc();
  e->kernel_event_address = ak_id;
  e->event_node = event_node;
  return e;
}


static void
clear_recursive
(
 void
)
{
  if (!ak_map_root) {
    return;
  }
  active_kernels_delete(ak_map_root->kernel_event_address);
  // after deletion, parent of the removed node is taken to the top of the tree
  // so, either only the root will be deleted(since root has no parent) or
  // either the left or right child will be the root, confirm what happens with John
  clear_recursive();
}



//******************************************************************************
// interface operations
//******************************************************************************

void
active_kernels_insert
(
 uint64_t ak_id,
 event_node_t *event_node
)
{
  spinlock_lock(&ak_map_lock);
  if (ak_lookup(&ak_map_root, ak_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    active_kernels_entry_t *entry = ak_node_new(ak_id, event_node);
    ak_insert(&ak_map_root, entry);  
    size++;
  }
  spinlock_unlock(&ak_map_lock);
}


void
active_kernels_delete
(
 uint64_t ak_id
)
{
  spinlock_lock(&ak_map_lock);
  active_kernels_entry_t *node = ak_delete(&ak_map_root, ak_id);
  size--;
  ak_free(&ak_map_free_list, node);
  spinlock_unlock(&ak_map_lock);
}


void
active_kernels_forall
(
 splay_visit_t visit_type,
 void (*fn),
 void *arg
)
{
  ak_forall(ak_map_root, visit_type, fn, arg);
}


long
active_kernels_size
(
 void
)
{
  return size;
}


void
increment_blame_for_entry
(
 active_kernels_entry_t *entry,
 double blame
)
{
  entry->event_node->cpu_idle_blame += blame;
}


// what is the best way to delete the entire splay tree?
void
ak_map_clear
(
 void
)
{
  clear_recursive();		
}

