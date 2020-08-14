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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>
#include <string.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-handle-map.h"

#include "lib/prof-lean/splay-uint64.h"
#include "lib/prof-lean/spinlock.h"
#include "hpcrun/memory/hpcrun-malloc.h"
#include "hpcrun/gpu/gpu-splay-allocator.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"


#define st_insert				\
  typed_splay_insert(handle)

#define st_lookup				\
  typed_splay_lookup(handle)

#define st_delete				\
  typed_splay_delete(handle)

#define st_forall				\
  typed_splay_forall(handle)

#define st_count				\
  typed_splay_count(handle)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, level0_handle_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(handle) level0_handle_map_entry_t

typedef struct typed_splay_node(handle) {
  struct typed_splay_node(handle) *left;
  struct typed_splay_node(handle) *right;
  uint64_t level0_handle; // key
  level0_data_node_t* data;
} typed_splay_node(handle);

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(handle)


static level0_handle_map_entry_t *
level0_handle_map_entry_alloc(level0_handle_map_entry_t **free_list_ptr)
{
  return st_alloc(free_list_ptr);
}

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_handle_map_entry_t *
level0_handle_map_lookup
(
 level0_handle_map_entry_t** map_root_ptr,
 uint64_t key
)
{
  level0_handle_map_entry_t *result = st_lookup(map_root_ptr, key);
  return result;
}

void
level0_handle_map_insert
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t* new_entry
)
{
  st_insert(map_root_ptr, new_entry);
}

void
level0_handle_map_delete
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t** free_list_ptr,
 uint64_t key
)
{
  level0_handle_map_entry_t *node = st_delete(map_root_ptr, key);
  st_free(free_list_ptr, node);
}

level0_handle_map_entry_t*
level0_handle_map_entry_new
(
 level0_handle_map_entry_t **free_list,
 uint64_t key,
 level0_data_node_t* data
)
{
  level0_handle_map_entry_t *e = level0_handle_map_entry_alloc(free_list);

  memset(e, 0, sizeof(level0_handle_map_entry_t));
  e->level0_handle = key;
  e->data = data;

  return e;
}

level0_data_node_t**
level0_handle_map_entry_data_get
(
 level0_handle_map_entry_t *entry
)
{
  return &(entry->data);
}
