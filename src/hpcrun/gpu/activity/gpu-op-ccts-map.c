// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../common/lean/collections/splay-tree-entry-data.h"
#include "../../../common/lean/collections/freelist-entry-data.h"

#include "../../memory/hpcrun-malloc.h"

#include "gpu-op-ccts-map.h"



//******************************************************************************
// generic code - splay tree
//******************************************************************************

typedef struct gpu_op_ccts_map_entry_t {
  union {
    SPLAY_TREE_ENTRY_DATA(struct gpu_op_ccts_map_entry_t);
    FREELIST_ENTRY_DATA(struct gpu_op_ccts_map_entry_t);
  };
  uint64_t gpu_correlation_id;   // Key

  gpu_op_ccts_map_entry_value_t value;
} gpu_op_ccts_map_entry_t;


#define SPLAY_TREE_PREFIX         st
#define SPLAY_TREE_KEY_TYPE       uint64_t
#define SPLAY_TREE_KEY_FIELD      gpu_correlation_id
#define SPLAY_TREE_ENTRY_TYPE     gpu_op_ccts_map_entry_t
#include "../../../common/lean/collections/splay-tree.h"


#define FREELIST_ENTRY_TYPE       gpu_op_ccts_map_entry_t
#include "../../../common/lean/collections/freelist.h"



//******************************************************************************
// local data
//******************************************************************************

static __thread st_t cct_map = SPLAY_TREE_INITIALIZER;
static __thread freelist_t freelist = FREELIST_INIITALIZER(hpcrun_malloc_safe);



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_op_ccts_map_entry_value_t *
gpu_op_ccts_map_lookup
(
 uint64_t gpu_correlation_id
)
{
  gpu_op_ccts_map_entry_t *entry = st_lookup(&cct_map, gpu_correlation_id);

  return entry != NULL ? &entry->value : NULL;
}


bool
gpu_op_ccts_map_insert
(
 uint64_t gpu_correlation_id,
 gpu_op_ccts_map_entry_value_t value
)
{
  gpu_op_ccts_map_entry_t *entry = freelist_allocate(&freelist);

  *entry = (gpu_op_ccts_map_entry_t) {
    .gpu_correlation_id = gpu_correlation_id,
    .value = value
  };

  bool inserted = st_insert(&cct_map, entry);

  if (!inserted) {
    freelist_free(&freelist, entry);
  }

  return inserted;
}


bool
gpu_op_ccts_map_delete
(
 uint64_t gpu_correlation_id
)
{
  gpu_op_ccts_map_entry_t *entry = st_delete(&cct_map, gpu_correlation_id);

  if (entry == NULL) {
    return false;
  }

  freelist_free(&freelist, entry);
  return true;
}
