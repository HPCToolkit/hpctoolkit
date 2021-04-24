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
// Copyright ((c)) 2002-2021, Rice University
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

#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include "opencl-h2d-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../gpu-print.h"


#define st_insert				\
  typed_splay_insert(correlation_id)

#define st_lookup				\
  typed_splay_lookup(correlation_id)

#define st_delete				\
  typed_splay_delete(correlation_id)

#define st_forall				\
  typed_splay_forall(correlation_id)

#define st_count				\
  typed_splay_count(correlation_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, opencl_h2d_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(correlation_id) opencl_h2d_map_entry_t

typedef struct typed_splay_node(correlation_id) {
  struct typed_splay_node(correlation_id) *left;
  struct typed_splay_node(correlation_id) *right;
  uint64_t buffer_id; // key

  uint64_t corr_id;
  size_t size;
  opencl_object_t *cb_info;
} typed_splay_node(correlation_id); 


//******************************************************************************
// local data
//******************************************************************************

static opencl_h2d_map_entry_t *map_root = NULL;

static opencl_h2d_map_entry_t *free_list = NULL;

static spinlock_t opencl_h2d_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(correlation_id)


static opencl_h2d_map_entry_t *
opencl_h2d_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static opencl_h2d_map_entry_t *
opencl_h2d_map_entry_new
(
 uint64_t buffer_id,
 uint64_t correlation_id,
 size_t size,
 opencl_object_t *cb_info
)
{
  opencl_h2d_map_entry_t *e = opencl_h2d_map_entry_alloc();

  e->buffer_id = buffer_id;
  e->corr_id = correlation_id;
  e->size = size;
  e->cb_info = cb_info;

  return e;
}


//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_h2d_map_entry_t *
opencl_h2d_map_lookup
(
 uint64_t buffer_id
)
{
  spinlock_lock(&opencl_h2d_map_lock);

  uint64_t id = buffer_id;
  opencl_h2d_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&opencl_h2d_map_lock);

  return result;
}


void
opencl_h2d_map_insert
(
 uint64_t buffer_id, 
 uint64_t correlation_id, 
 size_t size,
 opencl_object_t *cb_info
)
{
  spinlock_lock(&opencl_h2d_map_lock);

  opencl_h2d_map_entry_t *entry = st_lookup(&map_root, buffer_id);
  if (entry) {
    entry->corr_id = correlation_id;
    entry->size = size;
    entry->cb_info = cb_info;
  } else {
    opencl_h2d_map_entry_t *entry = 
      opencl_h2d_map_entry_new(buffer_id, correlation_id, size, cb_info);

    st_insert(&map_root, entry);
  }

  spinlock_unlock(&opencl_h2d_map_lock);
}


void
opencl_h2d_map_delete
(
 uint64_t buffer_id
)
{
  spinlock_lock(&opencl_h2d_map_lock);

  opencl_h2d_map_entry_t *node = st_delete(&map_root, buffer_id);
  st_free(&free_list, node);

  spinlock_unlock(&opencl_h2d_map_lock);
}


uint64_t
opencl_h2d_map_entry_buffer_id_get
(
 opencl_h2d_map_entry_t *entry
)
{
  return entry->buffer_id;
}


uint64_t
opencl_h2d_map_entry_correlation_get
(
 opencl_h2d_map_entry_t *entry
)
{
  return entry->corr_id;
}


size_t
opencl_h2d_map_entry_size_get
(
 opencl_h2d_map_entry_t *entry
)
{
  return entry->size;
}


opencl_object_t *
opencl_h2d_map_entry_callback_info_get
(
 opencl_h2d_map_entry_t *entry
)
{
  return entry->cb_info;
}


void
opencl_update_ccts_for_h2d_nodes
(
 opencl_splay_fn_t fn	
)
{
	st_forall(map_root, splay_inorder, fn, NULL);
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
opencl_h2d_map_count
(
 void
)
{
  return st_count(map_root);
}
