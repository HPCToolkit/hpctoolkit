// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <string.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-cct-analysis-map.h"
#include "../gpu-splay-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(cct)

#define st_lookup				\
  typed_splay_lookup(cct)

#define st_delete				\
  typed_splay_delete(cct)

#define st_forall				\
  typed_splay_forall(cct)

#define st_count				\
  typed_splay_count(cct)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(cct))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(cct) cupti_cct_analysis_map_entry_t 



//******************************************************************************
// type declarations
//******************************************************************************

#define MAX_STR_LEN 1024

struct cupti_cct_analysis_map_entry_s {
  struct cupti_cct_analysis_map_entry_s *left;
  struct cupti_cct_analysis_map_entry_s *right;
  uint64_t cct;

  cct_node_t *p1;
  cct_node_t *p2;
  cct_node_t *p3;
  cct_node_t *prev;

  ip_normalized_t function_id;
  size_t stack_length;
  char function_name[MAX_STR_LEN];
  size_t count;
  size_t tool_depth;
  size_t api_depth;
  size_t grid_dim_x;
  size_t grid_dim_y;
  size_t grid_dim_z;
  size_t block_dim_x;
  size_t block_dim_y;
  size_t block_dim_z;
}; 

//******************************************************************************
// local data
//******************************************************************************

static __thread cupti_cct_analysis_map_entry_t *map_root = NULL;
static __thread int map_size = 0;
static cupti_cct_analysis_map_entry_t *free_list = NULL;

//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(cct)


static cupti_cct_analysis_map_entry_t *
cupti_cct_analysis_map_entry_new
(
 cct_node_t *cct,
 cct_node_t *p1,
 cct_node_t *p2,
 cct_node_t *p3,
 cct_node_t *prev,
 ip_normalized_t function_id,
 size_t stack_length,
 size_t tool_depth,
 size_t api_depth,
 char *function_name,
 uint32_t grid_dim_x,
 uint32_t grid_dim_y,
 uint32_t grid_dim_z,
 uint32_t block_dim_x,
 uint32_t block_dim_y,
 uint32_t block_dim_z
)
{
  cupti_cct_analysis_map_entry_t *e;
  e = st_alloc(&free_list);

  memset(e, 0, sizeof(cupti_cct_analysis_map_entry_t));

  e->cct = (uint64_t)cct;
  e->p1 = p1;
  e->p2 = p2;
  e->p3 = p3;
  e->prev = prev;
  e->function_id = function_id;
  e->stack_length = stack_length;
  e->tool_depth = tool_depth;
  e->api_depth = api_depth;
  size_t str_len = strlen(function_name);
  size_t copy_len = MAX_STR_LEN > str_len ? str_len : MAX_STR_LEN - 1;
  strncpy(e->function_name, function_name, copy_len);
  e->grid_dim_x = grid_dim_x;
  e->grid_dim_y = grid_dim_y;
  e->grid_dim_z = grid_dim_z;
  e->block_dim_x = block_dim_x;
  e->block_dim_y = block_dim_y;
  e->block_dim_z = block_dim_z;
  e->count = 1;

  return e;
}


static void
dump_fn_helper
(
 cupti_cct_analysis_map_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  ip_normalized_t ip1 = {
    .lm_id = 0,
    .lm_ip = 0
  };
  ip_normalized_t ip2= {
    .lm_id = 0,
    .lm_ip = 0
  };
  ip_normalized_t ip3 = {
    .lm_id = 0,
    .lm_ip = 0
  };
  if (entry->p1 != NULL) {
    ip1 = hpcrun_cct_addr(entry->p1)->ip_norm;
  }
  if (entry->p2 != NULL) {
    ip2 = hpcrun_cct_addr(entry->p2)->ip_norm;
  }
  if (entry->p3 != NULL) {
    ip3 = hpcrun_cct_addr(entry->p3)->ip_norm;
  }
  printf("%p, %p, %u, %p, %s, %zu, %zu, %zu, %zu, %zu, %zu, %zu, %zu, %zu, %zu, %d, %p, %d, %p, %d, %p\n", 
    (void *)entry->cct, (void *)entry->prev, entry->function_id.lm_id, (void *)entry->function_id.lm_ip,
    entry->function_name, entry->stack_length, entry->tool_depth, entry->api_depth,
    entry->grid_dim_x / entry->count, entry->grid_dim_y / entry->count, entry->grid_dim_z / entry->count,
    entry->block_dim_x / entry->count, entry->block_dim_y / entry->count, entry->block_dim_z / entry->count,
    entry->count, ip1.lm_id, (void *)ip1.lm_ip, ip2.lm_id, (void *)ip2.lm_ip, ip3.lm_id, (void *)ip3.lm_ip);
}


//******************************************************************************
// interface operations
//******************************************************************************

cupti_cct_analysis_map_entry_t *
cupti_cct_analysis_map_lookup
(
 cct_node_t *cct
)
{
  cupti_cct_analysis_map_entry_t *result = st_lookup(&map_root, (uint64_t)cct);

  return result;
}


void
cupti_cct_analysis_map_insert
(
 cct_node_t *cct,
 cct_node_t *p1,
 cct_node_t *p2,
 cct_node_t *p3,
 cct_node_t *prev,
 ip_normalized_t function_id,
 size_t stack_length,
 size_t tool_depth,
 size_t api_depth,
 char *function_name,
 uint32_t grid_dim_x,
 uint32_t grid_dim_y,
 uint32_t grid_dim_z,
 uint32_t block_dim_x,
 uint32_t block_dim_y,
 uint32_t block_dim_z
)
{
  cupti_cct_analysis_map_entry_t *entry = st_lookup(&map_root, (uint64_t)cct);

  if (entry == NULL) {
    entry = cupti_cct_analysis_map_entry_new(cct, p1, p2, p3, prev,
      function_id, stack_length, tool_depth, api_depth,
      function_name, grid_dim_x, grid_dim_y, grid_dim_z, block_dim_x, block_dim_y, block_dim_z);
    st_insert(&map_root, entry);
    ++map_size;
  } 

  entry->count += 1;
  entry->grid_dim_x += grid_dim_x;
  entry->grid_dim_y += grid_dim_y;
  entry->grid_dim_z += grid_dim_z;
  entry->block_dim_x += block_dim_x;
  entry->block_dim_y += block_dim_y;
  entry->block_dim_z += block_dim_z;
}


void
cupti_cct_analysis_map_dump
(
)
{
  printf("cct, prev, cubin_id, function_pc, function_name, stack_length, "
    "tool_depth, api_depth, "
    "grid_dim_x, grid_dim_y, grid_dim_z,"
    "block_dim_x, block_dim_y, block_dim_z, cnt, p1, p2, p3\n");
  st_forall(map_root, splay_inorder, dump_fn_helper, NULL);
}


size_t
cupti_cct_analysis_map_size_get
(
)
{
  return map_size;
}

