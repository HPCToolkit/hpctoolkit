//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "papi-kernel-map.h"

#include "../../../../gpu-splay-allocator.h"
#include "../../../../../../common/lean/splay-uint64.h"
#include "../../../../../../common/lean/spinlock.h"
#include "../../../../../messages/messages.h"



//******************************************************************************
// type declarations
//******************************************************************************

#define papi_kernel_insert \
  typed_splay_insert(papi_kernel_node)

#define papi_kernel_lookup \
  typed_splay_lookup(papi_kernel_node)

#define papi_kernel_delete \
  typed_splay_delete(papi_kernel_node)

#define papi_kernel_forall \
  typed_splay_forall(papi_kernel_node)

#define papi_kernel_count \
  typed_splay_count(papi_kernel_node)

#define papi_kernel_alloc(free_list) \
  typed_splay_alloc(free_list, papi_kernel_map_entry_t)

#define papi_kernel_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(papi_kernel_node) papi_kernel_map_entry_t

typedef struct typed_splay_node(papi_kernel_node) {
  struct typed_splay_node(papi_kernel_node) *left;
  struct typed_splay_node(papi_kernel_node) *right;
  uint64_t kernel_id; // key

  papi_kernel_node_t *node;
} typed_splay_node(papi_kernel_node);

typed_splay_impl(papi_kernel_node);



//******************************************************************************
// local data
//******************************************************************************

static papi_kernel_map_entry_t *kernel_map_root = NULL;
static papi_kernel_map_entry_t *kernel_map_free_list = NULL;

static spinlock_t kernel_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static papi_kernel_map_entry_t *
kernel_node_alloc()
{
  return papi_kernel_alloc(&kernel_map_free_list);
}


static papi_kernel_map_entry_t *
kernel_node_new
(
 uint64_t kernel_id,
 papi_kernel_node_t *node
)
{
  papi_kernel_map_entry_t *e = kernel_node_alloc();
  e->kernel_id = kernel_id;
  e->node = node;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

papi_kernel_map_entry_t*
papi_kernel_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_map_lock);
  papi_kernel_map_entry_t *result = papi_kernel_lookup(&kernel_map_root, kernel_id);
  spinlock_unlock(&kernel_map_lock);
  return result;
}


void
papi_kernel_map_insert
(
 uint64_t kernel_id,
 papi_kernel_node_t *node
)
{
  spinlock_lock(&kernel_map_lock);
  if (papi_kernel_lookup(&kernel_map_root, kernel_id)) {
    spinlock_unlock(&kernel_map_lock);
    assert(false && "entry for a given key should be inserted only once");
    hpcrun_terminate();
  }
  papi_kernel_map_entry_t *entry = kernel_node_new(kernel_id, node);
  papi_kernel_insert(&kernel_map_root, entry);
  spinlock_unlock(&kernel_map_lock);
}


void
papi_kernel_map_delete
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_map_lock);
  papi_kernel_map_entry_t *node = papi_kernel_delete(&kernel_map_root, kernel_id);
  if (node) {
    papi_kernel_free(&kernel_map_free_list, node);
  }
  spinlock_unlock(&kernel_map_lock);
}


papi_kernel_node_t*
papi_kernel_map_entry_kernel_node_get
(
 papi_kernel_map_entry_t *entry
)
{
  return entry->node;
}
