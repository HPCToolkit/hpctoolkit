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

#include "gtpin-correlation-id-map.h"
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-splay-allocator.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>


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
  typed_splay_alloc(free_list, gtpin_correlation_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(correlation_id) gtpin_correlation_id_map_entry_t

typedef struct typed_splay_node(correlation_id) {
  struct typed_splay_node(correlation_id) *left;
  struct typed_splay_node(correlation_id) *right;
  uint64_t gtpin_correlation_id; // key

  gpu_op_ccts_t op_ccts;
  gpu_activity_channel_t *activity_channel;
  uint64_t submit_time;
} typed_splay_node(correlation_id); 


//******************************************************************************
// local data
//******************************************************************************

static gtpin_correlation_id_map_entry_t *map_root = NULL;

static gtpin_correlation_id_map_entry_t *free_list = NULL;

static spinlock_t gtpin_correlation_id_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(correlation_id)


static gtpin_correlation_id_map_entry_t *
gtpin_correlation_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gtpin_correlation_id_map_entry_t *
gtpin_correlation_id_map_entry_new
(
 uint64_t gtpin_correlation_id, 
 gpu_op_ccts_t *op_ccts,
 gpu_activity_channel_t *activity_channel,
 uint64_t submit_time
)
{
  gtpin_correlation_id_map_entry_t *e = gtpin_correlation_id_map_entry_alloc();

  e->gtpin_correlation_id = gtpin_correlation_id;
  e->op_ccts = *op_ccts;
  e->activity_channel = activity_channel;
  e->submit_time = submit_time;

  return e;
}


//*****************************************************************************
// interface operations
//*****************************************************************************

gtpin_correlation_id_map_entry_t *
gtpin_correlation_id_map_lookup
(
 uint64_t gtpin_correlation_id
)
{
  spinlock_lock(&gtpin_correlation_id_map_lock);

  uint64_t correlation_id = gtpin_correlation_id;
  gtpin_correlation_id_map_entry_t *result = st_lookup(&map_root, correlation_id);

  spinlock_unlock(&gtpin_correlation_id_map_lock);

  return result;
}


void
gtpin_correlation_id_map_insert
(
 uint64_t gtpin_correlation_id, 
 gpu_op_ccts_t *op_ccts,
 gpu_activity_channel_t *activity_channel,
 uint64_t submit_time
)
{
  spinlock_lock(&gtpin_correlation_id_map_lock);

  gtpin_correlation_id_map_entry_t *entry = st_lookup(&map_root, gtpin_correlation_id);
  if (entry) {
    entry->op_ccts = *op_ccts;
    entry->activity_channel = activity_channel;
    entry->submit_time = submit_time;
  } else {
    gtpin_correlation_id_map_entry_t *entry = 
      gtpin_correlation_id_map_entry_new(gtpin_correlation_id, op_ccts, activity_channel, submit_time);

    st_insert(&map_root, entry);
  }

  spinlock_unlock(&gtpin_correlation_id_map_lock);
}


void
gtpin_correlation_id_map_delete
(
 uint64_t gtpin_correlation_id
)
{
  spinlock_lock(&gtpin_correlation_id_map_lock);

  gtpin_correlation_id_map_entry_t *node = st_delete(&map_root, gtpin_correlation_id);
  st_free(&free_list, node);

  spinlock_unlock(&gtpin_correlation_id_map_lock);
}


gpu_op_ccts_t
gtpin_correlation_id_map_entry_op_ccts_get
(
 gtpin_correlation_id_map_entry_t *entry
)
{
  return entry->op_ccts;
}


gpu_activity_channel_t *
gtpin_correlation_id_map_entry_activity_channel_get
(
 gtpin_correlation_id_map_entry_t *entry
)
{
  return entry->activity_channel;
}


uint64_t
gtpin_correlation_id_map_entry_submit_time_get
(
 gtpin_correlation_id_map_entry_t *entry
)
{
  return entry->submit_time; 
}


//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gtpin_correlation_id_map_count
(
 void
)
{
  return st_count(map_root);
}
