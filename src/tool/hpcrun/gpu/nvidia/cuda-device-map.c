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

#include <stdio.h>
#include <string.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cuda-device-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define st_insert				\
  typed_splay_insert(device)

#define st_lookup				\
  typed_splay_lookup(device)

#define st_delete				\
  typed_splay_delete(device)

#define st_forall				\
  typed_splay_forall(device)

#define st_count				\
  typed_splay_count(device)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, cuda_device_map_entry_t)

#define st_free(free_list, node)		\
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

