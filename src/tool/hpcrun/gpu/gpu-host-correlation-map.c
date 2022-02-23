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
// Copyright ((c)) 2002-2022, Rice University
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

#include <hpcrun/cct/cct.h>

#include "gpu-host-correlation-map.h"
#include "gpu-op-placeholders.h"
#include "gpu-splay-allocator.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "gpu-print.h"


#define st_insert				\
  typed_splay_insert(host_correlation)

#define st_lookup				\
  typed_splay_lookup(host_correlation)

#define st_delete				\
  typed_splay_delete(host_correlation)

#define st_forall				\
  typed_splay_forall(host_correlation)

#define st_count				\
  typed_splay_count(host_correlation)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_host_correlation_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//******************************************************************************
// type declarations
//******************************************************************************

#undef typed_splay_node
#define typed_splay_node(host_correlation) gpu_host_correlation_map_entry_t

typedef struct typed_splay_node(host_correlation) {
  struct typed_splay_node(host_correlation) *left;
  struct typed_splay_node(host_correlation) *right;

  uint64_t host_correlation_id; // key

  gpu_op_ccts_t gpu_op_ccts;

  uint64_t cpu_submit_time;

  gpu_activity_channel_t *activity_channel;

  int samples;
  int total_samples;
} typed_splay_node(host_correlation);



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_host_correlation_map_entry_t *map_root = NULL;

static __thread gpu_host_correlation_map_entry_t *free_list = NULL;

static __thread bool allow_replace = false;

//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(host_correlation)


static gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_entry_alloc
(
 void
)
{
  return st_alloc(&free_list);
}


static gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_entry_new
(
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_op_ccts,
 uint64_t cpu_submit_time,
 gpu_activity_channel_t *activity_channel
)
{
  gpu_host_correlation_map_entry_t *e = gpu_host_correlation_map_entry_alloc();

  memset(e, 0, sizeof(gpu_host_correlation_map_entry_t));

  e->host_correlation_id = host_correlation_id;
  e->gpu_op_ccts = *gpu_op_ccts;
  e->cpu_submit_time = cpu_submit_time;
  e->activity_channel = activity_channel;

  return e;
}


static bool
gpu_host_correlation_map_samples_pending
(
 uint64_t host_correlation_id,
 gpu_host_correlation_map_entry_t *entry
)
{
  bool result = true;
  PRINT("total %d curr %d\n", entry->total_samples, entry->samples);
  if (entry->samples == entry->total_samples) {
    gpu_host_correlation_map_delete(host_correlation_id);
    result = false;
  }
  return result;
}



//******************************************************************************
// interface operations
//******************************************************************************

gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_lookup
(
 uint64_t host_correlation_id
)
{
  gpu_host_correlation_map_entry_t *result = st_lookup(&map_root, host_correlation_id);

  PRINT("host_correlation_map lookup: id=0x%lx (entry %p) (&map_root=%p) tid=%llu\n",
	host_correlation_id, result, &map_root, (uint64_t) pthread_self());

  return result;
}


void
gpu_host_correlation_map_insert
(
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_op_ccts,
 uint64_t cpu_submit_time,
 gpu_activity_channel_t *activity_channel
)
{
  gpu_host_correlation_map_entry_t *entry = st_lookup(&map_root, host_correlation_id);
  if (entry) {
    if (allow_replace) {
      entry->gpu_op_ccts = *gpu_op_ccts;
      entry->cpu_submit_time = cpu_submit_time;
      entry->activity_channel = activity_channel;
    } else {
      // fatal error: host_correlation id already present; a
      // correlation should be inserted only once.
      assert(0);
    }
  } else {
    gpu_host_correlation_map_entry_t *entry =
      gpu_host_correlation_map_entry_new(host_correlation_id, gpu_op_ccts,
					 cpu_submit_time, activity_channel);

    st_insert(&map_root, entry);

    PRINT("host_correlation_map insert: correlation_id=0x%lx "
	 "activity_channel=%p (entry=%p) (&map_root=%p) tid=%llu\n",
	  host_correlation_id, activity_channel, entry, &map_root,
	  (uint64_t) pthread_self());
  }
}


bool
gpu_host_correlation_map_samples_increase
(
 uint64_t host_correlation_id,
 int val
)
{
  bool result = true;

  PRINT("correlation_map samples update: correlation_id=0x%lx (update %d)\n",
	host_correlation_id, val);

  gpu_host_correlation_map_entry_t *entry = st_lookup(&map_root, host_correlation_id);

  if (entry) {
    entry->samples += val;
    gpu_host_correlation_map_samples_pending(host_correlation_id, entry);
  }

  return result;
}


bool
gpu_host_correlation_map_total_samples_update
(
 uint64_t host_correlation_id,
 int val
)
{
  bool result = true;

  PRINT("correlation_map total samples update: correlation_id=0x%lx (update %d)\n",
       host_correlation_id, val);

  gpu_host_correlation_map_entry_t *entry = st_lookup(&map_root, host_correlation_id);

  if (entry) {
    entry->total_samples = val;
    result = gpu_host_correlation_map_samples_pending(host_correlation_id, entry);
  }

  return result;
}


void
gpu_host_correlation_map_delete
(
 uint64_t host_correlation_id
)
{
  PRINT("host_correlation_map delete: correlation_id=0x%lx\n", host_correlation_id);
  gpu_host_correlation_map_entry_t *node = st_delete(&map_root, host_correlation_id);
  st_free(&free_list, node);
}


gpu_activity_channel_t *
gpu_host_correlation_map_entry_channel_get
(
 gpu_host_correlation_map_entry_t *entry
)
{
  return entry->activity_channel;
}


cct_node_t *
gpu_host_correlation_map_entry_op_cct_get
(
 gpu_host_correlation_map_entry_t *entry,
 gpu_placeholder_type_t ph_type
)
{
  return entry->gpu_op_ccts.ccts[ph_type];
}


cct_node_t *
gpu_host_correlation_map_entry_op_function_get
(
 gpu_host_correlation_map_entry_t *entry
)
{
  return entry->gpu_op_ccts.ccts[gpu_placeholder_type_kernel];
}


uint64_t
gpu_host_correlation_map_entry_cpu_submit_time
(
 gpu_host_correlation_map_entry_t *entry
)
{
  return entry->cpu_submit_time;
}


void
gpu_host_correlation_map_replace_set
(
 bool replace
)
{
  allow_replace = replace;
}


//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gpu_host_correlation_map_count
(
 void
)
{
  return st_count(map_root);
}
