// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _hpctoolkit_uw_hash_h_
#define _hpctoolkit_uw_hash_h_

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../common/lean/binarytree.h"

#include "unwind-interval.h"
#include "uw_recipe_map.h"



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct {
  unwinder_t uw;
  void *key;
  ilmstat_btuwi_pair_t *ilm_btui;
  bitree_uwi_t *btuwi;
} uw_hash_entry_t;

typedef struct {
  size_t size;
  uw_hash_entry_t *uw_hash_entries;
} uw_hash_table_t;

typedef void *(*uw_hash_malloc_fn)(size_t size);



//*****************************************************************************
// interface operations
//*****************************************************************************

uw_hash_table_t *
uw_hash_new
(
  size_t size,
  uw_hash_malloc_fn fn
);

void
uw_hash_insert
(
  uw_hash_table_t *uw_hash_table,
  unwinder_t uw,
  void *key,
  ilmstat_btuwi_pair_t *ilm_btui,
  bitree_uwi_t *btuwi
);

uw_hash_entry_t *
uw_hash_lookup
(
  uw_hash_table_t *uw_hash_table,
  unwinder_t uw,
  void *key
);

void
uw_hash_delete_range
(
  uw_hash_table_t *uw_hash_table,
  void *start,
  void *end
);

void
uw_hash_delete
(
  uw_hash_table_t *uw_hash_table,
  void *key
);

#endif // _hpctoolkit_uw_hash_h_
