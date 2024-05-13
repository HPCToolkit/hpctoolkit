// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _hpctoolkit_hash_h_
#define _hpctoolkit_hash_h_

//******************************************************************************
//
// map for recording directed blame for locks, critical sections, ...
//
//******************************************************************************


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

typedef struct {
  uint64_t key;
  uint64_t value;
} hash_entry_t;


typedef struct {
  size_t size;
  hash_entry_t *hash_entries;
} hash_table_t;

/***************************************************************************
 * interface operations
 ***************************************************************************/

typedef void *(*hash_malloc_fn)(size_t size);

hash_table_t *hash_new(size_t size, hash_malloc_fn fn);

void hash_insert(hash_table_t *hash_table, uint64_t key, uint64_t value);

hash_entry_t *hash_lookup(hash_table_t *hash_table, uint64_t key);

#endif // _hpctoolkit_hash_h_
