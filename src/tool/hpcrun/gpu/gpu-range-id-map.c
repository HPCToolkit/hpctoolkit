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

#include <lib/prof-lean/splay-uint64.h>
#include <hpcrun/thread_data.h>

#include "gpu-range-id-map.h"
#include "gpu-splay-allocator.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "gpu-print.h"


#define st_insert				\
  typed_splay_insert(range_id)

#define st_lookup				\
  typed_splay_lookup(range_id)

#define st_delete				\
  typed_splay_delete(range_id)

#define st_forall				\
  typed_splay_forall(range_id)

#define st_count				\
  typed_splay_count(range_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_range_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(range_id) gpu_range_id_map_entry_t

typedef struct typed_splay_node(range_id) {
  struct typed_splay_node(range_id) *left;
  struct typed_splay_node(range_id) *right;
  uint64_t range_id; // key
  
  thread_data_t *thread_data;
} typed_splay_node(range_id); 





//******************************************************************************
// local data
//******************************************************************************

static gpu_range_id_map_entry_t *map_root = NULL;

static gpu_range_id_map_entry_t *free_list = NULL;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(range_id)


static gpu_range_id_map_entry_t *
gpu_range_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gpu_range_id_map_entry_t *
gpu_range_id_map_entry_new
(
 uint32_t range_id, 
 thread_data_t *thread_data
)
{
  gpu_range_id_map_entry_t *e = gpu_range_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_range_id_map_entry_t)); 

  e->range_id = range_id;
  e->thread_data = thread_data;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_range_id_map_entry_t *
gpu_range_id_map_lookup
(
 uint32_t range_id
)
{
  gpu_range_id_map_entry_t *result = st_lookup(&map_root, (uint64_t)range_id);

  PRINT("range_id map lookup: id=0x%lx (record %p)\n", 
       range_id, result);

  return result;
}


void
gpu_range_id_map_insert
(
 uint32_t range_id, 
 thread_data_t *thread_data
)
{
  if (st_lookup(&map_root, range_id)) { 
    assert(0);
  } else {
    gpu_range_id_map_entry_t *entry = 
      gpu_range_id_map_entry_new(range_id, thread_data);

    st_insert(&map_root, entry);

    PRINT("range_id_map insert: range_id=0x%lx thread_data=%ld (entry=%p)\n", 
      range_id, thread_data, entry);
  }
}


void
gpu_range_id_map_delete
(
 uint32_t range_id
)
{
  gpu_range_id_map_entry_t *node = st_delete(&map_root, range_id);
  st_free(&free_list, node);
}


thread_data_t *
gpu_range_id_map_entry_thread_data_get
(
 gpu_range_id_map_entry_t *entry
)
{
  return entry->thread_data;
}

//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gpu_range_id_map_count
(
 void
)
{
  return st_count(map_root);
}

