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

#include "simdgroup-map.h"



//******************************************************************************
// type declarations
//******************************************************************************

#define sg_insert \
  typed_splay_insert(simdgroup)

#define sg_lookup \
  typed_splay_lookup(simdgroup)

#define sg_delete \
  typed_splay_delete(simdgroup)

#define sg_forall \
  typed_splay_forall(simdgroup)

#define sg_count \
  typed_splay_count(simdgroup)

#define sg_alloc(free_list) \
  typed_splay_alloc(free_list, simdgroup_map_entry_t)

#define sg_free(free_list, node) \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(simdgroup) simdgroup_map_entry_t

typedef struct typed_splay_node(simdgroup) {
  struct typed_splay_node(simdgroup) *left;
  struct typed_splay_node(simdgroup) *right;
  uint64_t simdgroup_id; // key

  bool maskCtrl;
  uint32_t execMask;
  GenPredArgs predArgs;
} typed_splay_node(simdgroup);

typed_splay_impl(simdgroup);



//******************************************************************************
// local data
//******************************************************************************

static simdgroup_map_entry_t *simdgroup_map_root = NULL;
static simdgroup_map_entry_t *simdgroup_map_free_list = NULL;

static spinlock_t simdgroup_map_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

static simdgroup_map_entry_t *
simdgroup_alloc()
{
  return sg_alloc(&simdgroup_map_free_list);
}


static simdgroup_map_entry_t *
simdgroup_new
(
 uint64_t simdgroup_id,
 bool maskCtrl,
 uint32_t execMask,
 GenPredArgs predArgs
)
{
  simdgroup_map_entry_t *e = simdgroup_alloc();
  e->simdgroup_id = simdgroup_id;
  e->maskCtrl = maskCtrl;
  e->execMask = execMask;
  e->predArgs = predArgs;
  return e;
}



//******************************************************************************
// interface operations
//******************************************************************************

simdgroup_map_entry_t*
simdgroup_map_lookup
(
 uint64_t simdgroup_id
)
{
  spinlock_lock(&simdgroup_map_lock);
  simdgroup_map_entry_t *result = sg_lookup(&simdgroup_map_root, simdgroup_id);
  spinlock_unlock(&simdgroup_map_lock);
  return result;
}


simdgroup_map_entry_t*
simdgroup_map_insert
(
 uint64_t simdgroup_id,
 bool maskCtrl,
 uint32_t execMask,
 GenPredArgs predArgs
)
{
  if (sg_lookup(&simdgroup_map_root, simdgroup_id)) {
    assert(0);  // entry for a given key should be inserted only once
  } else {
    spinlock_lock(&simdgroup_map_lock);
    simdgroup_map_entry_t *entry = simdgroup_new(simdgroup_id, maskCtrl, execMask, predArgs);
    sg_insert(&simdgroup_map_root, entry);
    spinlock_unlock(&simdgroup_map_lock);
    return entry;
  }
}


void
simdgroup_map_delete
(
 uint64_t simdgroup_id
)
{
  spinlock_lock(&simdgroup_map_lock);
  simdgroup_map_entry_t *node = sg_delete(&simdgroup_map_root, simdgroup_id);
  sg_free(&simdgroup_map_free_list, node);
  spinlock_unlock(&simdgroup_map_lock);
}


uint32_t
simdgroup_entry_getExecMask
(
 simdgroup_map_entry_t *entry
)
{
  return entry->execMask;
}


bool
simdgroup_entry_getMaskCtrl
(
 simdgroup_map_entry_t *entry
)
{
  return entry->maskCtrl;
}


GenPredArgs
simdgroup_entry_getPredArgs
(
 simdgroup_map_entry_t *entry
)
{
  return entry->predArgs;
}

