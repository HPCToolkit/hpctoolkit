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
// Copyright ((c)) 2002-2019, Rice University
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

#include "gpu-device-id-map.h"
#include "gpu-splay-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "gpu-print.h"

#define st_insert				\
  typed_splay_insert(device_id)

#define st_lookup				\
  typed_splay_lookup(device_id)

#define st_delete				\
  typed_splay_delete(device_id)

#define st_forall				\
  typed_splay_forall(device_id)

#define st_count				\
  typed_splay_count(device_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_device_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)


//******************************************************************************
// type declarations
//******************************************************************************
#undef typed_splay_node
#define typed_splay_node(correlation_id) gpu_device_id_map_entry_t

typedef struct typed_splay_node(device_id) { 
  struct gpu_device_id_map_entry_t *left;
  struct gpu_device_id_map_entry_t *right;
  uint64_t device_id;

  // average properties
  uint32_t core_clock_rate;
  uint32_t core_clock_rate_instance;
  uint32_t sm_clock_rate;
  uint32_t mem_clock_rate;
  uint32_t fan_speed;
  uint32_t temperature;
  uint32_t power;
} typed_splay_node(device_id); 


//******************************************************************************
// local data
//******************************************************************************

static gpu_device_id_map_entry_t *map_root = NULL;
static gpu_device_id_map_entry_t *free_list = NULL;

//******************************************************************************
// private operations
//******************************************************************************

typed_splay_impl(device_id)


static gpu_device_id_map_entry_t *
gpu_device_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gpu_device_id_map_entry_t *
gpu_device_id_map_entry_new(uint32_t device_id)
{
  gpu_device_id_map_entry_t *e = gpu_device_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_device_id_map_entry_t));

  e->device_id = device_id;

  return e;
}


//******************************************************************************
// interface operations
//******************************************************************************

gpu_device_id_map_entry_t *
gpu_device_id_map_lookup
(
 uint32_t device_id
)
{
  gpu_device_id_map_entry_t *result = st_lookup(&map_root, device_id);

  TMSG(DEFER_CTXT, "event map lookup: event=0x%lx (record %p)", 
       device_id, result);

  return result;
}


void
gpu_device_id_map_insert
(
 uint32_t device_id,
 uint32_t core_clock_rate
)
{
  gpu_device_id_map_entry_t *entry = st_lookup(&map_root, device_id);

  if (!entry) {
    entry = gpu_device_id_map_entry_new(device_id);
    st_insert(&map_root, entry);
  }

  if (core_clock_rate != 0) {
    entry->core_clock_rate += core_clock_rate;
    entry->core_clock_rate_instance += 1;
  }

  PRINT("device_id_map insert: device_id=0x%lx\n", device_id);
}


void
gpu_device_id_map_delete
(
 uint32_t device_id
)
{
  gpu_device_id_map_entry_t *node = st_delete(&map_root, device_id);
  st_free(&free_list, node);
}


double
gpu_device_id_map_entry_core_clock_rate_get
(
 gpu_device_id_map_entry_t *entry
)
{
  return entry->core_clock_rate / entry->core_clock_rate_instance;
}

