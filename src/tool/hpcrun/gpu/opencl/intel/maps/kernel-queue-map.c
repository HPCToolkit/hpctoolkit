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

#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>
#include <lib/prof-lean/spinlock.h>
#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <hpcrun/memory/hpcrun-malloc.h>    // hpcrun_malloc_safe

#include "kernel-queue-map.h"



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
  typed_splay_alloc(free_list, kernel_queue_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) kernel_queue_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t kernel_id; // key

  qc_node_t *qc_list;
} typed_splay_node(queue); 


//******************************************************************************
// local data
//******************************************************************************

static kernel_queue_map_entry_t *map_root = NULL;
static kernel_queue_map_entry_t *free_list = NULL;

static qc_node_t *qc_node_free_list = NULL;
static spinlock_t kernel_queue_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static kernel_queue_map_entry_t *
kernel_queue_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static kernel_queue_map_entry_t *
kernel_queue_map_entry_new
(
 uint64_t kernel_id
)
{
  kernel_queue_map_entry_t *e = kernel_queue_map_entry_alloc();

  e->kernel_id = kernel_id;
  e->qc_list = NULL;
  
  return e;
}


static qc_node_t*
qc_node_alloc_helper
(
 qc_node_t **free_list
)
{
  qc_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->next;
  } else {
    first = (qc_node_t *) hpcrun_malloc_safe(sizeof(qc_node_t));
  }

  memset(first, 0, sizeof(qc_node_t));
  return first;
}


static void
qc_node_free_helper
(
 qc_node_t **free_list, 
 qc_node_t *node 
)
{
  node->next = *free_list;
  *free_list = node;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

kernel_queue_map_entry_t *
kernel_queue_map_lookup
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_queue_map_lock);

  uint64_t id = kernel_id;
  kernel_queue_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&kernel_queue_map_lock);

  return result;
}


// called on clEnqueueNDRangeKernel
kernel_queue_map_entry_t*
kernel_queue_map_insert
(
 uint64_t kernel_id, 
 uint64_t queue_id, 
 uint64_t context_id
)
{
  spinlock_lock(&kernel_queue_map_lock);

  kernel_queue_map_entry_t *entry = st_lookup(&map_root, kernel_id);
  if (!entry) {
    entry = kernel_queue_map_entry_new(kernel_id);
    st_insert(&map_root, entry);
    entry->qc_list = NULL;
  }

  qc_node_t *node = qc_node_alloc_helper(&qc_node_free_list);
  node->queue_id = queue_id;
  node->context_id = context_id;
  node->next = entry->qc_list;
  entry->qc_list = node;

  spinlock_unlock(&kernel_queue_map_lock);
  return entry;
}


// called on clReleaseKernel
void
kernel_queue_map_delete
(
 uint64_t kernel_id
)
{
  spinlock_lock(&kernel_queue_map_lock);
  
  kernel_queue_map_entry_t *node = st_delete(&map_root, kernel_id);
  // clear all nodes inside node->qc_list
  qc_node_t *qn = node->qc_list;
  qc_node_t *next;
  while (qn) {
    next = qn->next;
    qc_node_free_helper(&qc_node_free_list, qn);
    qn = next;
  }
  st_free(&free_list, node);

  spinlock_unlock(&kernel_queue_map_lock);
}


uint64_t
kernel_queue_map_entry_kernel_id_get
(
 kernel_queue_map_entry_t *entry
)
{
  return entry->kernel_id;
}


qc_node_t*
kernel_queue_map_entry_qc_list_get
(
 kernel_queue_map_entry_t *entry
)
{
  return entry->qc_list;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
kernel_queue_map_count
(
 void
)
{
  return st_count(map_root);
}
