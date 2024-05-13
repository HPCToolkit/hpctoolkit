// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   cuda-device-map.c
//
// Purpose:
//   implementation of a map that enables one to look up device
//   properties given a device id
//
//***************************************************************************


//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../../common/lean/splay-uint64.h"

#include "../../../messages/messages.h"
#include "../../../memory/hpcrun-malloc.h"

#include "cuda-device-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define st_insert                               \
  typed_splay_insert(device)

#define st_lookup                               \
  typed_splay_lookup(device)

#define st_delete                               \
  typed_splay_delete(device)

#define st_forall                               \
  typed_splay_forall(device)

#define st_count                                \
  typed_splay_count(device)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, cuda_device_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(device) cuda_device_map_entry_t



//*****************************************************************************
// type declarations
//*****************************************************************************

struct cuda_device_map_entry_t {
  struct cuda_device_map_entry_t *left;
  struct cuda_device_map_entry_t *right;
  uint64_t device; // key (must be 64 bits to use splay-uint64 base)

  cuda_device_property_t property;
};



//*****************************************************************************
// global data
//*****************************************************************************

static cuda_device_map_entry_t *map_root = NULL;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(device)


static cuda_device_map_entry_t *
cuda_device_map_entry_new
(
 uint32_t device
)
{
  cuda_device_map_entry_t *e = (cuda_device_map_entry_t *)
    hpcrun_malloc_safe(sizeof(cuda_device_map_entry_t));

  memset(e, 0, sizeof(cuda_device_map_entry_t));

  e->device = device;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

cuda_device_map_entry_t *
cuda_device_map_lookup
(
 uint32_t id
)
{
  cuda_device_map_entry_t *result = st_lookup(&map_root, id);

  TMSG(CUDA_DEVICE, "device map lookup: id=0x%lx (record %p)", id, result);

  return result;
}


void
cuda_device_map_insert
(
 uint32_t device
)
{
  cuda_device_map_entry_t *entry = cuda_device_map_entry_new(device);
  cuda_device_property_query(device, &entry->property);

  TMSG(CUDA_DEVICE, "device map insert: id=0x%lx (record %p)", device, entry);

  st_insert(&map_root, entry);
}


void
cuda_device_map_delete
(
 uint32_t device
)
{
  st_delete(&map_root, device);
}


cuda_device_property_t *
cuda_device_map_entry_device_property_get
(
 cuda_device_map_entry_t *entry
)
{
  return &entry->property;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

int
cuda_device_map_count
(
 void
)
{
  return st_count(map_root);
}
