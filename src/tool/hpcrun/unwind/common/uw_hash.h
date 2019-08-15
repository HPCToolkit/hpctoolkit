// -*-Mode: C++;-*- // technically C99

#ifndef _hpctoolkit_uw_hash_h_
#define _hpctoolkit_uw_hash_h_

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


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/binarytree.h>

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
