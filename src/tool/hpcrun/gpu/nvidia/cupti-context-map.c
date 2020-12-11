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
#include <lib/prof-lean/spinlock.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-context-map.h"
#include "../gpu-splay-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(context)

#define st_lookup				\
  typed_splay_lookup(context)

#define st_delete				\
  typed_splay_delete(context)

#define st_forall				\
  typed_splay_forall(context)

#define st_count				\
  typed_splay_count(context)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(context))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(context) cupti_context_map_entry_t 



//******************************************************************************
// type declarations
//******************************************************************************

struct cupti_context_map_entry_s {
  struct cupti_context_map_entry_s *left;
  struct cupti_context_map_entry_s *right;
  uint64_t context;

  // PC sampling records
  size_t num_stall_reasons;
  uint32_t *stall_reason_index;
  char **stall_reason_names;
  size_t num_buffer_pcs;
  cupti_pc_sampling_data_t *pc_sampling_data;
}; 



//******************************************************************************
// local data
//******************************************************************************

static cupti_context_map_entry_t *map_root = NULL;
static cupti_context_map_entry_t *free_list = NULL;
static spinlock_t spinlock = SPINLOCK_UNLOCKED;

//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(context)


static cupti_context_map_entry_t *
cupti_context_map_entry_new
(
 CUcontext context
)
{
  cupti_context_map_entry_t *e;
  e = st_alloc(&free_list);

  memset(e, 0, sizeof(cupti_context_map_entry_t));

  return e;
}



static void
flush_fn_helper
(
 cupti_context_map_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  cupti_context_process_args_t *process_args = (cupti_context_process_args_t *)args;
  cupti_context_process_fn_t fn = (cupti_context_process_fn_t)process_args->fn;
  fn(entry->context, process_args->args);
}


//******************************************************************************
// interface operations
//******************************************************************************

cupti_context_map_entry_t *
cupti_context_map_lookup
(
 CUcontext context
)
{
  spinlock_lock(&spinlock);

  cupti_context_map_entry_t *result = st_lookup(&map_root, (uint64_t)context);

  TMSG(DEFER_CTXT, "context map lookup: context=0x%lx (record %p)", 
       context, result);

  spinlock_unlock(&spinlock);

  return result;
}


void
cupti_context_map_init
(
 CUcontext context
)
{
  spinlock_lock(&spinlock);

  cupti_context_map_entry_t *entry = st_lookup(&map_root, (uint64_t)context);

  if (entry == NULL) {
    entry = cupti_context_map_entry_new(context);
    st_insert(&map_root, entry);
  }

  spinlock_unlock(&spinlock);
}


void
cupti_context_map_pc_sampling_insert
(
 CUcontext context,
 size_t num_stall_reasons,
 uint32_t *stall_reason_index,
 char **stall_reason_names,
 size_t num_buffer_pcs,
 cupti_pc_sampling_data_t *pc_sampling_data
)
{
  spinlock_lock(&spinlock);

  cupti_context_map_entry_t *entry = st_lookup(&map_root, (uint64_t)context);

  if (entry != NULL) {
    entry->num_stall_reasons = num_stall_reasons;
    entry->stall_reason_index = stall_reason_index;
    entry->stall_reason_names = stall_reason_names;
    entry->num_buffer_pcs = num_buffer_pcs;
    entry->pc_sampling_data = pc_sampling_data;
  }

  spinlock_unlock(&spinlock);
}


void
cupti_context_map_delete
(
 CUcontext context
)
{
  spinlock_lock(&spinlock);

  cupti_context_map_entry_t *node = st_delete(&map_root, (uint64_t)context);

  st_free(&free_list, node);

  spinlock_unlock(&spinlock);
}


void
cupti_context_map_process
(
 cupti_context_process_fn_t fn,
 void *args
)
{
  cupti_context_process_args_t process_args = {
    .fn = fn,
    .args = args
  };
  st_forall(map_root, splay_inorder, flush_fn_helper, &process_args);
}


cupti_pc_sampling_data_t *
cupti_context_map_entry_buffer_pc_get
(
 cupti_context_map_entry_t *entry
)
{
  return entry->pc_sampling_data;
}


size_t
cupti_context_map_entry_num_stall_reasons_get
(
 cupti_context_map_entry_t *entry
)
{
  return entry->num_stall_reasons;
}


uint32_t *
cupti_context_map_entry_stall_reason_index_get
(
 cupti_context_map_entry_t *entry
)
{
  return entry->stall_reason_index;
}


char **
cupti_context_map_entry_stall_reason_names_get
(
 cupti_context_map_entry_t *entry
)
{
  return entry->stall_reason_names;
}
