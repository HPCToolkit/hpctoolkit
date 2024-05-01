//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../../../lib/prof-lean/collections/splay-tree-entry-data.h"
#include "../../../../../lib/prof-lean/collections/freelist-entry-data.h"

#include "../../../memory/hpcrun-malloc.h"

#include "cuda-kernel-data-map.h"



//******************************************************************************
// generic code - splay tree
//******************************************************************************

typedef struct cuda_kernel_data_map_entry_t {
  union {
    SPLAY_TREE_ENTRY_DATA(struct cuda_kernel_data_map_entry_t);
    FREELIST_ENTRY_DATA(struct cuda_kernel_data_map_entry_t);
  };
  uint64_t gpu_correlation_id;   // Key

  cuda_kernel_data_map_entry_value_t value;
} cuda_kernel_data_map_entry_t;


#define SPLAY_TREE_PREFIX         st
#define SPLAY_TREE_KEY_TYPE       uint64_t
#define SPLAY_TREE_KEY_FIELD      gpu_correlation_id
#define SPLAY_TREE_ENTRY_TYPE     cuda_kernel_data_map_entry_t
#define SPLAY_TREE_DEFINE_INPLACE
#include "../../../../../lib/prof-lean/collections/splay-tree.h"


#define FREELIST_ENTRY_TYPE       cuda_kernel_data_map_entry_t
#include "../../../../../lib/prof-lean/collections/freelist.h"



//******************************************************************************
// local data
//******************************************************************************

static st_t kernel_data_map = SPLAY_TREE_INITIALIZER;
static freelist_t freelist = FREELIST_INIITALIZER(hpcrun_malloc_safe);



//*****************************************************************************
// interface operations
//*****************************************************************************

cuda_kernel_data_map_entry_value_t *
cuda_kernel_data_map_lookup
(
 uint64_t gpu_correlation_id
)
{
  cuda_kernel_data_map_entry_t *entry =
    st_lookup(&kernel_data_map, gpu_correlation_id);

  return entry != NULL ? &entry->value : NULL;
}


bool
cuda_kernel_data_map_insert
(
 uint64_t gpu_correlation_id,
 cuda_kernel_data_map_entry_value_t value
)
{
  cuda_kernel_data_map_entry_t *entry = freelist_allocate(&freelist);

  *entry = (cuda_kernel_data_map_entry_t) {
    .gpu_correlation_id = gpu_correlation_id,
    .value = value
  };

  bool inserted = st_insert(&kernel_data_map, entry);

  if (!inserted) {
    freelist_free(&freelist, entry);
  }

  return inserted;
}


bool
cuda_kernel_data_map_delete
(
 uint64_t gpu_correlation_id
)
{
  cuda_kernel_data_map_entry_t *entry =
    st_delete(&kernel_data_map, gpu_correlation_id);

  if (entry == NULL) {
    return false;
  }

  freelist_free(&freelist, entry);
  return true;
}
