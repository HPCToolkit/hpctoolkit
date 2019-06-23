// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

#include <assert.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cuda-device-map.h"



//*****************************************************************************
// type definitions 
//*****************************************************************************

struct cuda_device_map_entry_s {
  uint32_t device;
  cuda_device_property_t property;
  struct cuda_device_map_entry_s *left;
  struct cuda_device_map_entry_s *right;
}; 



//*****************************************************************************
// global data 
//*****************************************************************************

static cuda_device_map_entry_t *cuda_device_map_root = NULL;



//*****************************************************************************
// private operations
//*****************************************************************************

static cuda_device_map_entry_t *
cuda_device_map_entry_new
(
 uint32_t device
)
{
  cuda_device_map_entry_t *e = (cuda_device_map_entry_t *) 
    hpcrun_malloc_safe(sizeof(cuda_device_map_entry_t));

  e->device = device;
  e->left = NULL;
  e->right = NULL;

  return e;
}


static cuda_device_map_entry_t *
cuda_device_map_splay
(
 cuda_device_map_entry_t *root, uint32_t key
)
{
  REGULAR_SPLAY_TREE(cuda_device_map_entry_s, root, key, device, left, right);
  return root;
}


static void
cuda_device_map_delete_root
(
 void
)
{
  TMSG(DEFER_CTXT, "device %d: delete", cuda_device_map_root->device);

  if (cuda_device_map_root->left == NULL) {
    cuda_device_map_root = cuda_device_map_root->right;
  } else {
    cuda_device_map_root->left = 
      cuda_device_map_splay(cuda_device_map_root->left, 
			   cuda_device_map_root->device);
    cuda_device_map_root->left->right = cuda_device_map_root->right;
    cuda_device_map_root = cuda_device_map_root->left;
  }
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
  cuda_device_map_entry_t *result = NULL;

  cuda_device_map_root = cuda_device_map_splay(cuda_device_map_root, id);
  if (cuda_device_map_root && cuda_device_map_root->device == id) {
    result = cuda_device_map_root;
  }

  TMSG(DEFER_CTXT, "device map lookup: id=0x%lx (record %p)", id, result);
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

  TMSG(DEFER_CTXT, "device map insert: id=0x%lx (record %p)", device, entry);

  entry->left = entry->right = NULL;

  if (cuda_device_map_root != NULL) {
    cuda_device_map_root = 
      cuda_device_map_splay(cuda_device_map_root, device);

    if (device < cuda_device_map_root->device) {
      entry->left = cuda_device_map_root->left;
      entry->right = cuda_device_map_root;
      cuda_device_map_root->left = NULL;
    } else if (device > cuda_device_map_root->device) {
      entry->left = cuda_device_map_root;
      entry->right = cuda_device_map_root->right;
      cuda_device_map_root->right = NULL;
    } else {
      // device already present: fatal error since a device 
      //   should only be inserted once 
      assert(0);
    }
  }
  cuda_device_map_root = entry;
}


void
cuda_device_map_delete
(
 uint32_t device
)
{
  cuda_device_map_root =
    cuda_device_map_splay(cuda_device_map_root, device);

  if (cuda_device_map_root &&
    cuda_device_map_root->device == device) {
    cuda_device_map_delete_root();
  }
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

static int 
cuda_device_map_count_helper
(
 cuda_device_map_entry_t *entry
) 
{
  if (entry) {
     int left = cuda_device_map_count_helper(entry->left);
     int right = cuda_device_map_count_helper(entry->right);
     return 1 + right + left; 
  } 

  return 0;
}


int 
cuda_device_map_count
(
 void
)
{
  return cuda_device_map_count_helper(cuda_device_map_root);
}

