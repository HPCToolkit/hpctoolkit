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
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cubin-crc-map.h"



//*****************************************************************************
// macros
//*****************************************************************************


#define CUPTI_CUBIN_CRC_MAP_HASH_TABLE_SIZE 127



//*****************************************************************************
// type definitions
//*****************************************************************************

struct cubin_crc_map_entry_s {
  uint64_t cubin_crc;
  uint32_t hpctoolkit_module_id;
  Elf_SymbolVector *elf_vector;
  struct cubin_crc_map_entry_s *left;
  struct cubin_crc_map_entry_s *right;
};


typedef struct {
  uint64_t cubin_crc;
  cubin_crc_map_entry_t *entry;
} cubin_crc_map_hash_entry_t;



//*****************************************************************************
// global data
//*****************************************************************************

static cubin_crc_map_entry_t *cubin_crc_map_root = NULL;
static spinlock_t cubin_crc_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

static cubin_crc_map_entry_t *
cubin_crc_map_entry_new(uint64_t cubin_crc, Elf_SymbolVector *vector)
{
  cubin_crc_map_entry_t *e;
  e = (cubin_crc_map_entry_t *)hpcrun_malloc_safe(sizeof(cubin_crc_map_entry_t));
  e->cubin_crc = cubin_crc;
  e->left = NULL;
  e->right = NULL;
  e->elf_vector = vector;

  return e;
}


static cubin_crc_map_entry_t *
cubin_crc_map_splay(cubin_crc_map_entry_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(cubin_crc_map_entry_s, root, key, cubin_crc, left, right);
  return root;
}


static void
cubin_crc_map_delete_root()
{
  TMSG(CUDA_CUBIN, "cubin_crc %d: delete", cubin_crc_map_root->cubin_crc);

  if (cubin_crc_map_root->left == NULL) {
    cubin_crc_map_root = cubin_crc_map_root->right;
  } else {
    cubin_crc_map_root->left =
      cubin_crc_map_splay(cubin_crc_map_root->left,
        cubin_crc_map_root->cubin_crc);
    cubin_crc_map_root->left->right = cubin_crc_map_root->right;
    cubin_crc_map_root = cubin_crc_map_root->left;
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

cubin_crc_map_entry_t *
cubin_crc_map_lookup
(
 uint64_t id
)
{
  cubin_crc_map_entry_t *result = NULL;

  static __thread cubin_crc_map_hash_entry_t *cubin_crc_map_hash_table = NULL;

  if (cubin_crc_map_hash_table == NULL) {
    cubin_crc_map_hash_table = (cubin_crc_map_hash_entry_t *)hpcrun_malloc_safe(
      CUPTI_CUBIN_CRC_MAP_HASH_TABLE_SIZE * sizeof(cubin_crc_map_hash_entry_t));
    memset(cubin_crc_map_hash_table, 0, CUPTI_CUBIN_CRC_MAP_HASH_TABLE_SIZE *
      sizeof(cubin_crc_map_hash_entry_t));
  }

  cubin_crc_map_hash_entry_t *hash_entry = &(cubin_crc_map_hash_table[id % CUPTI_CUBIN_CRC_MAP_HASH_TABLE_SIZE]);

  if (hash_entry->entry == NULL || hash_entry->cubin_crc != id) {
    spinlock_lock(&cubin_crc_map_lock);

    cubin_crc_map_root = cubin_crc_map_splay(cubin_crc_map_root, id);
    if (cubin_crc_map_root && cubin_crc_map_root->cubin_crc == id) {
      result = cubin_crc_map_root;
    }

    spinlock_unlock(&cubin_crc_map_lock);

    hash_entry->cubin_crc = id;
    hash_entry->entry = result;
  } else {
    result = hash_entry->entry;
  }

  TMSG(CUDA_CUBIN, "cubin_crc map lookup: id=0x%lx (record %p)", id, result);
  return result;
}


void
cubin_crc_map_insert
(
 uint64_t cubin_crc,
 uint32_t hpctoolkit_module_id,
 Elf_SymbolVector *vector
)
{
  spinlock_lock(&cubin_crc_map_lock);

  if (cubin_crc_map_root != NULL) {
    cubin_crc_map_root =
      cubin_crc_map_splay(cubin_crc_map_root, cubin_crc);

    if (cubin_crc < cubin_crc_map_root->cubin_crc) {
      cubin_crc_map_entry_t *entry = cubin_crc_map_entry_new(cubin_crc, vector);
      TMSG(CUDA_CUBIN, "cubin_crc map insert: id=0x%lx (record %p)", cubin_crc, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_crc_map_root->left;
      entry->right = cubin_crc_map_root;
      cubin_crc_map_root->left = NULL;
      cubin_crc_map_root = entry;
    } else if (cubin_crc > cubin_crc_map_root->cubin_crc) {
      cubin_crc_map_entry_t *entry = cubin_crc_map_entry_new(cubin_crc, vector);
      TMSG(CUDA_CUBIN, "cubin_crc map insert: id=0x%lx (record %p)", cubin_crc, entry);
      entry->left = entry->right = NULL;
      entry->hpctoolkit_module_id = hpctoolkit_module_id;
      entry->left = cubin_crc_map_root;
      entry->right = cubin_crc_map_root->right;
      cubin_crc_map_root->right = NULL;
      cubin_crc_map_root = entry;
    } else {
      // cubin_crc already present
    }
  } else {
    cubin_crc_map_entry_t *entry = cubin_crc_map_entry_new(cubin_crc, vector);
    entry->hpctoolkit_module_id = hpctoolkit_module_id;
    cubin_crc_map_root = entry;
  }

  spinlock_unlock(&cubin_crc_map_lock);
}


void
cubin_crc_map_delete
(
 uint64_t cubin_crc
)
{
  cubin_crc_map_root = cubin_crc_map_splay(cubin_crc_map_root, cubin_crc);
  if (cubin_crc_map_root && cubin_crc_map_root->cubin_crc == cubin_crc) {
    cubin_crc_map_delete_root();
  }
}


uint32_t
cubin_crc_map_entry_hpctoolkit_id_get
(
 cubin_crc_map_entry_t *entry
)
{
  return entry->hpctoolkit_module_id;
}


Elf_SymbolVector *
cubin_crc_map_entry_elf_vector_get
(
 cubin_crc_map_entry_t *entry
)
{
  return entry->elf_vector;
}


//--------------------------------------------------------------------------
// Transform a <cubin_crc, function_index, offset> tuple to a pc address by
// looking up elf symbols inside a cubin
//--------------------------------------------------------------------------
ip_normalized_t
cubin_crc_transform(uint64_t cubin_crc, uint32_t function_index, uint64_t offset)
{
  cubin_crc_map_entry_t *entry = cubin_crc_map_lookup(cubin_crc);
  ip_normalized_t ip;
  TMSG(CUDA_CUBIN, "cubin_crc %lu", cubin_crc);
  if (entry != NULL) {
    uint32_t hpctoolkit_module_id = cubin_crc_map_entry_hpctoolkit_id_get(entry);
    TMSG(CUDA_CUBIN, "get hpctoolkit_module_id %d", hpctoolkit_module_id);
    const Elf_SymbolVector *vector = cubin_crc_map_entry_elf_vector_get(entry);
    ip.lm_id = (uint16_t)hpctoolkit_module_id;
    ip.lm_ip = (uintptr_t)(vector->symbols[function_index] + offset);
  }
  return ip;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

static int
cubin_crc_map_count_helper
(
 cubin_crc_map_entry_t *entry
)
{
  if (entry) {
    int left = cubin_crc_map_count_helper(entry->left);
    int right = cubin_crc_map_count_helper(entry->right);
    return 1 + right + left;
  }
  return 0;
}


int
cubin_crc_map_count
(
 void
)
{
  return cubin_crc_map_count_helper(cubin_crc_map_root);
}

