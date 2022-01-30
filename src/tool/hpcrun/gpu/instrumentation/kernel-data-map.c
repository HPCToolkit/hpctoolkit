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
// Copyright ((c)) 2002-2022, Rice University
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

#include "kernel-data-map.h"

#include "hpcrun/gpu/gpu-splay-allocator.h"

#include "lib/prof-lean/spinlock.h"
#include "lib/prof-lean/splay-uint64.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define kd_insert typed_splay_insert(kernel_data)

#define kd_lookup typed_splay_lookup(kernel_data)

#define kd_delete typed_splay_delete(kernel_data)

#define kd_forall typed_splay_forall(kernel_data)

#define kd_count typed_splay_count(kernel_data)

#define kd_alloc(free_list) typed_splay_alloc(free_list, kernel_data_map_entry_t)

#define kd_free(free_list, node) typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(kernel_data) kernel_data_map_entry_t

typedef struct typed_splay_node(kernel_data) {
  struct typed_splay_node(kernel_data) * left;
  struct typed_splay_node(kernel_data) * right;
  uint64_t kernel_id;  // key

  kernel_data_t kernel_data;
}
typed_splay_node(kernel_data);

typed_splay_impl(kernel_data);

static kernel_data_map_entry_t* kernel_data_map_root = NULL;
static kernel_data_map_entry_t* kernel_data_map_free_list = NULL;

static spinlock_t kernel_data_map_lock = SPINLOCK_UNLOCKED;

static kernel_data_map_entry_t* kernel_data_alloc() {
  return kd_alloc(&kernel_data_map_free_list);
}

static kernel_data_map_entry_t* kernel_data_new(uint64_t kernel_id, kernel_data_t kernel_data) {
  kernel_data_map_entry_t* e = kernel_data_alloc();
  e->kernel_id = kernel_id;
  e->kernel_data = kernel_data;
  return e;
}

kernel_data_map_entry_t* kernel_data_map_lookup(uint64_t kernel_id) {
  spinlock_lock(&kernel_data_map_lock);
  kernel_data_map_entry_t* result = kd_lookup(&kernel_data_map_root, kernel_id);
  spinlock_unlock(&kernel_data_map_lock);
  return result;
}

void kernel_data_map_insert(uint64_t kernel_id, kernel_data_t kernel_data) {
  if (kd_lookup(&kernel_data_map_root, kernel_id)) {
    hpcrun_terminate();
  } else {
    spinlock_lock(&kernel_data_map_lock);
    kernel_data_map_entry_t* entry = kernel_data_new(kernel_id, kernel_data);
    kd_insert(&kernel_data_map_root, entry);
    spinlock_unlock(&kernel_data_map_lock);
  }
}

void kernel_data_map_delete(uint64_t kernel_id) {
  kernel_data_map_entry_t* node = kd_delete(&kernel_data_map_root, kernel_id);
  kd_free(&kernel_data_map_free_list, node);
}

kernel_data_t kernel_data_map_entry_kernel_data_get(kernel_data_map_entry_t* entry) {
  return entry->kernel_data;
}
