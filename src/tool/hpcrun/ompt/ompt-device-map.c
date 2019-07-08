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
//   ompt-device-map.c
//
// Purpose:
//   implementation of map from device id to device data structure
//  
//***************************************************************************


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "ompt-device-map.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct ompt_device_map_entry_s {
  uint64_t device_id;
  uint64_t refcnt;
  ompt_device_t *device;
  const char *type;
  struct ompt_device_map_entry_s *left;
  struct ompt_device_map_entry_s *right;
}; 



//*****************************************************************************
// global data 
//*****************************************************************************

static ompt_device_map_entry_t *ompt_device_map_root = NULL;
static spinlock_t ompt_device_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

static ompt_device_map_entry_t *
ompt_device_map_entry_new
(
 uint64_t device_id, 
 ompt_device_t *device, 
 const char *type
)
{
  ompt_device_map_entry_t *e;
  e = (ompt_device_map_entry_t *)hpcrun_malloc(sizeof(ompt_device_map_entry_t));
  e->device_id = device_id;
  e->device = device;
  e->type = type;
  e->left = NULL;
  e->right = NULL;
  e->refcnt = 0;

  return e;
}


static ompt_device_map_entry_t *
ompt_device_map_splay
(
 ompt_device_map_entry_t *root, 
 uint64_t key
)
{
  REGULAR_SPLAY_TREE(ompt_device_map_entry_s, root, key, device_id, left, right);
  return root;
}


static void
ompt_device_map_delete_root
(
 void
)
{
  TMSG(DEFER_CTXT, "device %d: delete", ompt_device_map_root->device_id);

  if (ompt_device_map_root->left == NULL) {
    ompt_device_map_root = ompt_device_map_root->right;
  } else {
    ompt_device_map_root->left = 
      ompt_device_map_splay(ompt_device_map_root->left, 
			   ompt_device_map_root->device_id);
    ompt_device_map_root->left->right = ompt_device_map_root->right;
    ompt_device_map_root = ompt_device_map_root->left;
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

ompt_device_map_entry_t *
ompt_device_map_lookup
(
 uint64_t id
)
{
  ompt_device_map_entry_t *result = NULL;
  spinlock_lock(&ompt_device_map_lock);

  ompt_device_map_root = ompt_device_map_splay(ompt_device_map_root, id);
  if (ompt_device_map_root && ompt_device_map_root->device_id == id) {
    result = ompt_device_map_root;
  }

  spinlock_unlock(&ompt_device_map_lock);

  TMSG(DEFER_CTXT, "device map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
ompt_device_map_insert
(
 uint64_t device_id, 
 ompt_device_t *device, 
 const char *type
)
{
  ompt_device_map_entry_t *entry = ompt_device_map_entry_new(device_id, device, type);

  TMSG(DEFER_CTXT, "device map insert: id=0x%lx (record %p)", device_id, entry);

  entry->left = entry->right = NULL;

  spinlock_lock(&ompt_device_map_lock);

  if (ompt_device_map_root != NULL) {
    ompt_device_map_root = 
      ompt_device_map_splay(ompt_device_map_root, device_id);

    if (device_id < ompt_device_map_root->device_id) {
      entry->left = ompt_device_map_root->left;
      entry->right = ompt_device_map_root;
      ompt_device_map_root->left = NULL;
    } else if (device_id > ompt_device_map_root->device_id) {
      entry->left = ompt_device_map_root;
      entry->right = ompt_device_map_root->right;
      ompt_device_map_root->right = NULL;
    } else {
      // device_id already present: fatal error since a device_id 
      //   should only be inserted once 
      assert(0);
    }
  }
  ompt_device_map_root = entry;

  spinlock_unlock(&ompt_device_map_lock);
}


// return true if record found; false otherwise
bool
ompt_device_map_refcnt_update
(
 uint64_t device_id, 
 int val
)
{
  bool result = false; 

  TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (update %d)", 
       device_id, val);

  spinlock_lock(&ompt_device_map_lock);
  ompt_device_map_root = ompt_device_map_splay(ompt_device_map_root, device_id);

  if (ompt_device_map_root && 
      ompt_device_map_root->device_id == device_id) {
    uint64_t old = ompt_device_map_root->refcnt;
    ompt_device_map_root->refcnt += val;
    TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (%ld --> %ld)",
      device_id, old, ompt_device_map_root->refcnt);
    if (ompt_device_map_root->refcnt == 0) {
      TMSG(DEFER_CTXT, "device map refcnt_update: id=0x%lx (deleting)",
        device_id);
      ompt_device_map_delete_root();
    }
    result = true;
  }

  spinlock_unlock(&ompt_device_map_lock);
  return result;
}


ompt_device_t *
ompt_device_map_entry_device_get
(
 ompt_device_map_entry_t *entry
)
{
  return entry->device;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

static int 
ompt_device_map_count_helper
(
 ompt_device_map_entry_t *entry
) 
{
  if (entry) {
     int left = ompt_device_map_count_helper(entry->left);
     int right = ompt_device_map_count_helper(entry->right);
     return 1 + right + left; 
  } 
  return 0;
}


int 
ompt_device_map_count
(
 void
) 
{
  return ompt_device_map_count_helper(ompt_device_map_root);
}


