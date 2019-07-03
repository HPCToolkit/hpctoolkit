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

#include "uw_hash.h"

/***************************************************************************
 * private operations
 ***************************************************************************/

#define UW_HASH(key, size) (key < size ? key : key % size);

/***************************************************************************
 * interface operations
 ***************************************************************************/

uw_hash_table_t*
uw_hash_new(size_t size, uw_hash_malloc_fn fn)
{
  uw_hash_table_t *uw_hash_table = (uw_hash_table_t *)fn(sizeof(uw_hash_table_t));
  uw_hash_entry_t *uw_hash_entries = (uw_hash_entry_t *)fn(size * sizeof(uw_hash_entry_t));
  memset(uw_hash_entries, 0, size * sizeof(uw_hash_entry_t));
  uw_hash_table->size = size;
  uw_hash_table->uw_hash_entries = uw_hash_entries;
  return uw_hash_table;
}


void
uw_hash_insert(uw_hash_table_t *uw_hash_table,
  void *key, ilmstat_btuwi_pair_t *ilm_btui, bitree_uwi_t *btuwi)
{
  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  uw_hash_entry->key = key;
  uw_hash_entry->ilm_btui = ilm_btui;
  uw_hash_entry->btuwi = btuwi;
}


void
uw_hash_ilm_btui_insert(uw_hash_table_t *uw_hash_table,
  void *key, ilmstat_btuwi_pair_t *ilm_btui)
{
  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  uw_hash_entry->ilm_btui = ilm_btui;
}


void
uw_hash_btuwi_insert(uw_hash_table_t *uw_hash_table,
  void *key, bitree_uwi_t *btuwi)
{
  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  uw_hash_entry->btuwi = btuwi;
}


uw_hash_entry_t *
uw_hash_lookup(uw_hash_table_t *uw_hash_table, void *key)
{
  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  if (uw_hash_entry->key != key) {
    return NULL;
  }
  return uw_hash_entry;
}


void
uw_hash_delete_range(uw_hash_table_t *uw_hash_table, void *start, void *end)
{
  size_t i;
  for (i = 0; i < uw_hash_table->size; ++i) {
    void *key = uw_hash_table->uw_hash_entries[i].key;
    if (key >= start && key < end) {
      uw_hash_table->uw_hash_entries[i].key = 0;
    }
  }
}


void
uw_hash_delete(uw_hash_table_t *uw_hash_table, void *key)
{
  size_t index = UW_HASH(((size_t)key), uw_hash_table->size);
  uw_hash_entry_t *uw_hash_entry = &(uw_hash_table->uw_hash_entries[index]);
  if (uw_hash_entry->key == key) {
    uw_hash_entry->key = NULL;
    uw_hash_entry->ilm_btui = NULL;
    uw_hash_entry->btuwi = NULL;
  }
}
