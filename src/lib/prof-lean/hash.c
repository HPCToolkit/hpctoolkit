// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
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

//
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <string.h>
#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "hash.h"

/***************************************************************************
 * private operations
 ***************************************************************************/

#define HASH(key, size) (key < size ? key : key % size);

/***************************************************************************
 * interface operations
 ***************************************************************************/

hash_table_t*
hash_new(size_t size, hash_malloc_fn fn)
{
  hash_table_t *hash_table = (hash_table_t *)fn(sizeof(hash_table_t));
  hash_entry_t *hash_entries = (hash_entry_t *)fn(size * sizeof(hash_entry_t));
  memset(hash_entries, 0, size * sizeof(hash_entry_t));
  hash_table->size = size;
  hash_table->hash_entries = hash_entries;
  return hash_table;
}


void
hash_insert(hash_table_t *hash_table, uint64_t key, uint64_t value)
{
  size_t index = HASH(key, hash_table->size);
  hash_entry_t *hash_entry = &(hash_table->hash_entries[index]);
  hash_entry->key = key;
  hash_entry->value = value;
}


hash_entry_t *
hash_lookup(hash_table_t *hash_table, uint64_t key)
{
  size_t index = HASH(key, hash_table->size);
  hash_entry_t *hash_entry = &(hash_table->hash_entries[index]);
  if (hash_entry->key == 0) {
    return NULL;
  }
  return hash_entry;
}
