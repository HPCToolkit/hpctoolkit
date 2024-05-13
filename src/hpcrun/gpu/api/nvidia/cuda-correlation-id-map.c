// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../../common/lean/collections/splay-tree-entry-data.h"
#include "../../../../common/lean/collections/freelist-entry-data.h"

#include "../../../memory/hpcrun-malloc.h"

#include "cuda-correlation-id-map.h"



//******************************************************************************
// generic code - splay tree
//******************************************************************************

typedef struct cuda_correlation_id_map_entry_t {
  union {
    SPLAY_TREE_ENTRY_DATA(struct cuda_correlation_id_map_entry_t);
    FREELIST_ENTRY_DATA(struct cuda_correlation_id_map_entry_t);
  };
  uint64_t gpu_correlation_id;  // Key

  uint64_t host_correlation_id;
  uint32_t samples_count;
  uint32_t total_samples;
};


#define SPLAY_TREE_PREFIX         st
#define SPLAY_TREE_KEY_TYPE       uint64_t
#define SPLAY_TREE_KEY_FIELD      gpu_correlation_id
#define SPLAY_TREE_ENTRY_TYPE     cuda_correlation_id_map_entry_t
#define SPLAY_TREE_DEFINE_INPLACE
#include "../../../../common/lean/collections/splay-tree.h"


#define FREELIST_ENTRY_TYPE       cuda_correlation_id_map_entry_t
#include "../../../../common/lean/collections/freelist.h"



//******************************************************************************
// local data
//******************************************************************************

static st_t correlations_map = SPLAY_TREE_INITIALIZER;
static freelist_t freelist = FREELIST_INIITALIZER(hpcrun_malloc_safe);



//******************************************************************************
// interface operations
//******************************************************************************

cuda_correlation_id_map_entry_t *
cuda_correlation_id_map_lookup
(
 uint64_t gpu_correlation_id
)
{
  return st_lookup(&correlations_map, gpu_correlation_id);
}


bool
cuda_correlation_id_map_insert
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
)
{
  cuda_correlation_id_map_entry_t *entry = freelist_allocate(&freelist);

  *entry = (cuda_correlation_id_map_entry_t) {
    .gpu_correlation_id = gpu_correlation_id,
    .host_correlation_id = host_correlation_id,
    .samples_count = 0,
    .total_samples = 0
  };

  bool inserted = st_insert(&correlations_map, entry);
  if (!inserted) {
    freelist_free(&freelist, entry);
  }

  return inserted;
}


bool
cuda_correlation_id_map_delete
(
 uint64_t gpu_correlation_id
)
{
  cuda_correlation_id_map_entry_t *entry =
    st_delete(&correlations_map, gpu_correlation_id);

  if (entry == NULL) {
    return false;
  }

  freelist_free(&freelist, entry);
  return true;
}


uint64_t
cuda_correlation_id_map_entry_get_host_id
(
 cuda_correlation_id_map_entry_t *entry
)
{
  return entry->host_correlation_id;
}


bool
cuda_correlation_id_map_entry_set_total_samples
(
 cuda_correlation_id_map_entry_t *entry,
 uint32_t total_samples
)
{
  entry->total_samples = total_samples;
  return entry->total_samples == entry->samples_count;
}


bool
cuda_correlation_id_map_entry_increment_samples
(
 cuda_correlation_id_map_entry_t *entry,
 uint32_t num_samples
)
{
  entry->samples_count += num_samples;
  return entry->total_samples == entry->samples_count;
}
