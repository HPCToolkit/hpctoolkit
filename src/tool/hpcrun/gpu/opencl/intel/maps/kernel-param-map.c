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

#include <string.h>                       // memset


//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>
#include <hpcrun/gpu/gpu-splay-allocator.h> // typed_splay_alloc
#include <hpcrun/memory/hpcrun-malloc.h>    // hpcrun_malloc_safe

#include "kernel-param-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

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
  typed_splay_alloc(free_list, kernel_param_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) kernel_param_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t kernel_id; // key

  kp_node_t *kp_list;
} typed_splay_node(queue); 


//******************************************************************************
// local data
//******************************************************************************

static kernel_param_map_entry_t *map_root = NULL;
static kernel_param_map_entry_t *free_list = NULL;

static kp_node_t *kp_node_free_list = NULL;
static spinlock_t kernel_param_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static kernel_param_map_entry_t *
kernel_param_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static kernel_param_map_entry_t *
kernel_param_map_entry_new
(
 uint64_t kernel_id
)
{
  kernel_param_map_entry_t *e = kernel_param_map_entry_alloc();

  e->kernel_id = kernel_id;
  e->kp_list = NULL;
  
  return e;
}


static kp_node_t*
kp_node_alloc_helper
(
 kp_node_t **free_list
)
{
  kp_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->next;
  } else {
    first = (kp_node_t *) hpcrun_malloc_safe(sizeof(kp_node_t));
  }

  memset(first, 0, sizeof(kp_node_t));
  return first;
}


static void
kp_node_free_helper
(
 kp_node_t **free_list, 
 kp_node_t *node 
)
{
  node->next = *free_list;
  *free_list = node;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

kernel_param_map_entry_t *
kernel_param_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_param_map_lock);

  uint64_t id = kernel_id;
  kernel_param_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&kernel_param_map_lock);

  return result;
}


// called on clSetKernelArg
kernel_param_map_entry_t*
kernel_param_map_insert
(
 uint64_t kernel_id, 
 const void *mem, 
 size_t size
)
{
  spinlock_lock(&kernel_param_map_lock);

  kernel_param_map_entry_t *entry = st_lookup(&map_root, kernel_id);
  if (!entry) {
    entry = kernel_param_map_entry_new(kernel_id);
    st_insert(&map_root, entry);
  }

  kp_node_t *node = kp_node_alloc_helper(&kp_node_free_list);
  node->mem = mem;
  node->size = size;
  node->next = entry->kp_list;
  entry->kp_list = node;

  spinlock_unlock(&kernel_param_map_lock);
  return entry;
}


// called on clReleaseKernel
void
kernel_param_map_delete
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_param_map_lock);
  
  kernel_param_map_entry_t *node = st_delete(&map_root, kernel_id);
  if (!node) {
    // This kernel could have no params or clReleaseKernel has been called more than once
    spinlock_unlock(&kernel_param_map_lock);
    return;
  }
  // clear all nodes inside node->kp_list
  kp_node_t *kn = node->kp_list;
  kp_node_t *next;
  while (kn) {
    next = kn->next;
    kp_node_free_helper(&kp_node_free_list, kn);
    kn = next;
  }
  st_free(&free_list, node);

  spinlock_unlock(&kernel_param_map_lock);
}


uint64_t
kernel_param_map_entry_kernel_id_get
(
 kernel_param_map_entry_t *entry
)
{
  return entry->kernel_id;
}


kp_node_t*
kernel_param_map_entry_kp_list_get
(
 kernel_param_map_entry_t *entry
)
{
  return entry->kp_list;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
kernel_param_map_count
(
 void
)
{
  return st_count(map_root);
}
