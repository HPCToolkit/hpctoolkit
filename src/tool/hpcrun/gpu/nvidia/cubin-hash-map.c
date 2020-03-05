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



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/crypto-hash.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-hash-map.h"



//*****************************************************************************
// type definitions 
//*****************************************************************************

struct cubin_hash_map_entry_s {
  uint32_t cubin_id;
  struct cubin_hash_map_entry_s *left;
  struct cubin_hash_map_entry_s *right;
  unsigned char hash[0];
}; 



//*****************************************************************************
// global data 
//*****************************************************************************

static cubin_hash_map_entry_t *cubin_hash_map_root = NULL;
static spinlock_t cubin_hash_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

static cubin_hash_map_entry_t *
cubin_hash_map_entry_new
(
 uint32_t cubin_id, 
 const void *cubin, 
 size_t size
)
{
  cubin_hash_map_entry_t *e;
  unsigned int hash_length = crypto_hash_length();
  e = (cubin_hash_map_entry_t *)
    hpcrun_malloc_safe(sizeof(cubin_hash_map_entry_t) + hash_length);
  e->cubin_id = cubin_id;
  e->left = NULL;
  e->right = NULL;
  crypto_hash_compute(cubin, size, e->hash, hash_length);

  return e;
}


static cubin_hash_map_entry_t *
cubin_hash_map_splay
(
 cubin_hash_map_entry_t *root, 
 uint32_t key
)
{
  REGULAR_SPLAY_TREE(cubin_hash_map_entry_s, root, key, cubin_id, left, right);
  return root;
}


static void
__attribute__ ((unused))
cubin_hash_map_delete_root
(
 void
)
{
  TMSG(DEFER_CTXT, "cubin_id %d: delete", cubin_hash_map_root->cubin_id);

  if (cubin_hash_map_root->left == NULL) {
    cubin_hash_map_root = cubin_hash_map_root->right;
  } else {
    cubin_hash_map_root->left = 
      cubin_hash_map_splay(cubin_hash_map_root->left, 
			   cubin_hash_map_root->cubin_id);
    cubin_hash_map_root->left->right = cubin_hash_map_root->right;
    cubin_hash_map_root = cubin_hash_map_root->left;
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

cubin_hash_map_entry_t *
cubin_hash_map_lookup
(
 uint32_t id
)
{
  cubin_hash_map_entry_t *result = NULL;
  spinlock_lock(&cubin_hash_map_lock);

  cubin_hash_map_root = cubin_hash_map_splay(cubin_hash_map_root, id);
  if (cubin_hash_map_root && cubin_hash_map_root->cubin_id == id) {
    result = cubin_hash_map_root;
  }

  spinlock_unlock(&cubin_hash_map_lock);

  TMSG(DEFER_CTXT, "cubin_id map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cubin_hash_map_insert
(
 uint32_t cubin_id, 
 const void *cubin, 
 size_t size
)
{
  spinlock_lock(&cubin_hash_map_lock);

  if (cubin_hash_map_root != NULL) {
    cubin_hash_map_root = 
      cubin_hash_map_splay(cubin_hash_map_root, cubin_id);

    if (cubin_id < cubin_hash_map_root->cubin_id) {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_hash_map_root->left;
      entry->right = cubin_hash_map_root;
      cubin_hash_map_root->left = NULL;
      cubin_hash_map_root = entry;
    } else if (cubin_id > cubin_hash_map_root->cubin_id) {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      entry->left = cubin_hash_map_root;
      entry->right = cubin_hash_map_root->right;
      cubin_hash_map_root->right = NULL;
      cubin_hash_map_root = entry;
    } else {
      // cubin_id already present
    }
  } else {
      cubin_hash_map_entry_t *entry = cubin_hash_map_entry_new(cubin_id, cubin, size);
      entry->left = entry->right = NULL;
      cubin_hash_map_root = entry;
  }

  spinlock_unlock(&cubin_hash_map_lock);
}


unsigned char *
cubin_hash_map_entry_hash_get
(
 cubin_hash_map_entry_t *entry, 
 unsigned int *len
)
{
  *len = crypto_hash_length();
  return entry->hash;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

static int 
cubin_hash_map_count_helper
(
 cubin_hash_map_entry_t *entry
) 
{
  if (entry) {
    int left = cubin_hash_map_count_helper(entry->left);
    int right = cubin_hash_map_count_helper(entry->right);
    return 1 + right + left; 
  } 
  return 0;
}


int 
cubin_hash_map_count
(
 void
) 
{
  return cubin_hash_map_count_helper(cubin_hash_map_root);
}

