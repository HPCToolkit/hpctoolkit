//******************************************************************************
// system includes
//******************************************************************************

#include <string.h>
#include <assert.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>

#include "kernel-data-map.h"

//******************************************************************************
// type declarations
//******************************************************************************

#define kd_insert \
  typed_splay_insert(kernel_data)

#define kd_lookup \
  typed_splay_lookup(kernel_data)

#define kd_delete \
  typed_splay_delete(kernel_data)

#define kd_forall \
  typed_splay_forall(kernel_data)

#define kd_count \
  typed_splay_count(kernel_data)

#define kd_alloc(free_list) \
  typed_splay_alloc(free_list, kernel_data_map_entry_t)

#define kd_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(kernel_data) kernel_data_map_entry_t

typedef struct typed_splay_node(kernel_data) {
  struct typed_splay_node(kernel_data) *left;
  struct typed_splay_node(kernel_data) *right;
  uint64_t kernel_id; // key

  kernel_data_t kernel_data;
} typed_splay_node(kernel_data);

typed_splay_impl(kernel_data);

//******************************************************************************
// local data
//******************************************************************************

static kernel_data_map_entry_t *kernel_data_map_root = NULL;
static kernel_data_map_entry_t *kernel_data_map_free_list = NULL;

static spinlock_t kernel_data_map_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// private operations
//******************************************************************************

static kernel_data_map_entry_t *
kernel_data_alloc()
{
  return kd_alloc(&kernel_data_map_free_list);
}


static kernel_data_map_entry_t *
kernel_data_new
(
 uint64_t kernel_id,
 kernel_data_t kernel_data
)
{
  kernel_data_map_entry_t *e = kernel_data_alloc();
  e->kernel_id = kernel_id;
  e->kernel_data = kernel_data;
  return e;
}

//******************************************************************************
// interface operations
//******************************************************************************

kernel_data_map_entry_t*
kernel_data_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_data_map_lock);
  kernel_data_map_entry_t *result = kd_lookup(&kernel_data_map_root, kernel_id);
  spinlock_unlock(&kernel_data_map_lock);
  return result;
}


void
kernel_data_map_insert
(
 uint64_t kernel_id,
 kernel_data_t kernel_data
)
{
  if (kd_lookup(&kernel_data_map_root, kernel_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&kernel_data_map_lock);
    kernel_data_map_entry_t *entry = kernel_data_new(kernel_id, kernel_data);
    kd_insert(&kernel_data_map_root, entry);  
    spinlock_unlock(&kernel_data_map_lock);
  }
}


void
kernel_data_map_delete
(
 uint64_t kernel_id
)
{
  kernel_data_map_entry_t *node = kd_delete(&kernel_data_map_root, kernel_id);
  kd_free(&kernel_data_map_free_list, node);
}


kernel_data_t
kernel_data_map_entry_kernel_data_get
(
 kernel_data_map_entry_t *entry
)
{
  return entry->kernel_data;
}
