// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
