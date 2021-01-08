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
#include <lib/prof-lean/spinlock.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include "opencl-queue-map.h"


//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "../gpu-print.h"


#define st_insert				\
  typed_splay_insert(queue)

#define st_lookup				\
  typed_splay_lookup(queue)

#define st_delete				\
  typed_splay_delete(queue)

#define st_forall				\
  typed_splay_forall(queue)

#define st_count				\
  typed_splay_count(queue)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, opencl_queue_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) opencl_queue_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t queue; // key

  uint32_t queue_id; // we save queue id as the stream id
  uint32_t context_id;
} typed_splay_node(queue); 


//******************************************************************************
// local data
//******************************************************************************

static opencl_queue_map_entry_t *map_root = NULL;

static opencl_queue_map_entry_t *free_list = NULL;

static spinlock_t opencl_queue_map_lock = SPINLOCK_UNLOCKED;

static uint32_t cl_queue_id = 0;

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static opencl_queue_map_entry_t *
opencl_cl_queue_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static opencl_queue_map_entry_t *
opencl_cl_queue_map_entry_new
(
 uint64_t queue,
 uint32_t context_id
)
{
  opencl_queue_map_entry_t *e = opencl_cl_queue_map_entry_alloc();

  e->queue = queue;
  e->queue_id = cl_queue_id;
  e->context_id = context_id;
  
  return e;
}


//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_queue_map_entry_t *
opencl_cl_queue_map_lookup
(
 uint64_t queue
)
{
  spinlock_lock(&opencl_queue_map_lock);

  uint64_t id = queue;
  opencl_queue_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&opencl_queue_map_lock);

  return result;
}


uint32_t
opencl_cl_queue_map_update
(
 uint64_t queue, 
 uint32_t context_id
)
{
  uint32_t ret_queue_id = 0;

  spinlock_lock(&opencl_queue_map_lock);

  opencl_queue_map_entry_t *entry = st_lookup(&map_root, queue);
  if (entry) {
    entry->queue = queue;
    entry->queue_id = cl_queue_id;
    entry->context_id = context_id;
  } else {
    opencl_queue_map_entry_t *entry = 
      opencl_cl_queue_map_entry_new(queue, context_id);

    st_insert(&map_root, entry);
  }
    
  // Update cl_queue_id
  ret_queue_id = cl_queue_id++;

  spinlock_unlock(&opencl_queue_map_lock);

  return ret_queue_id;
}


void
opencl_cl_queue_map_delete
(
 uint64_t queue
)
{
  spinlock_lock(&opencl_queue_map_lock);

  opencl_queue_map_entry_t *node = st_delete(&map_root, queue);
  st_free(&free_list, node);

  spinlock_unlock(&opencl_queue_map_lock);
}


uint32_t
opencl_cl_queue_map_entry_queue_id_get
(
 opencl_queue_map_entry_t *entry
)
{
  return entry->queue_id;
}


uint32_t
opencl_cl_queue_map_entry_context_id_get
(
 opencl_queue_map_entry_t *entry
)
{
  return entry->context_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
opencl_cl_queue_map_count
(
 void
)
{
  return st_count(map_root);
}
