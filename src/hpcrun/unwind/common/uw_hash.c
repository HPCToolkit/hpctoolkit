// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//**************************************************************************
// local includes
//**************************************************************************

#define _GNU_SOURCE

#include "uw_hash.h"



//**************************************************************************
// macros
//**************************************************************************

#define UW_HASH(key, size) (key < size ? key : key % size);

#define DISABLE_HASHTABLE 0



//**************************************************************************
// interface operations
//**************************************************************************

uw_hash_table_t*
uw_hash_new
(
  size_t size,
  uw_hash_malloc_fn fn
)
{
  uw_hash_table_t *uw_hash_table =
    (uw_hash_table_t *)fn(sizeof(uw_hash_table_t));

  uw_hash_entry_t *uw_hash_entries =
    (uw_hash_entry_t *)fn(size * sizeof(uw_hash_entry_t));

  memset(uw_hash_entries, 0, size * sizeof(uw_hash_entry_t));

  uw_hash_table->size = size;
  uw_hash_table->uw_hash_entries = uw_hash_entries;

  return uw_hash_table;
}


void
uw_hash_insert
(
  uw_hash_table_t *uw_hash_table,
  unwinder_t uw,
  void *key,
  ilmstat_btuwi_pair_t *ilm_btui,
  bitree_uwi_t *btuwi
)
{
#if DISABLE_HASHTABLE
  return;
#endif

  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  uw_hash_entry->uw = uw;
  uw_hash_entry->key = key;
  uw_hash_entry->ilm_btui = ilm_btui;
  uw_hash_entry->btuwi = btuwi;
}


uw_hash_entry_t *
uw_hash_lookup
(
  uw_hash_table_t *uw_hash_table,
  unwinder_t uw,
  void *key
)
{
#if DISABLE_HASHTABLE
  return NULL;
#endif

  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  if (uw_hash_entry->key != key || uw_hash_entry->uw != uw) {
    return NULL;
  }
  return uw_hash_entry;
}


void
uw_hash_delete_range
(
  uw_hash_table_t *uw_hash_table,
  void *start,
  void *end
)
{
#if DISABLE_HASHTABLE
  return;
#endif

  size_t i;
  for (i = 0; i < uw_hash_table->size; ++i) {
    void *key = uw_hash_table->uw_hash_entries[i].key;
    if (key >= start && key < end) {
      uw_hash_table->uw_hash_entries[i].key = 0;
    }
  }
}


void
uw_hash_delete
(
  uw_hash_table_t *uw_hash_table,
  void *key
)
{
#if DISABLE_HASHTABLE
  return;
#endif

  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  if (uw_hash_entry->key == key) {
    uw_hash_entry->key = NULL;
  }
}
