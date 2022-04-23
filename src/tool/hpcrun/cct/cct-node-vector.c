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

#include "cct-node-vector.h"

#include "hpcrun/memory/hpcrun-malloc.h"
#include "hpcrun/messages/messages.h"

#include <assert.h>

struct cct_node_vector_s {
  cct_node_t** nodes;
  uint64_t size;
  uint64_t capacity;
};

cct_node_vector_t* cct_node_vector_init() {
  cct_node_vector_t* vector = (cct_node_vector_t*)hpcrun_malloc(sizeof(cct_node_vector_t));
  vector->size = 0;
  vector->capacity = 0;
  cct_node_vector_reserve(vector, 32);
  return vector;
}

void cct_node_vector_reserve(cct_node_vector_t* vector, uint64_t capacity) {
  // FIXME(keren): free memory
  if (capacity > vector->capacity) {
    cct_node_t** new_nodes = (cct_node_t**)hpcrun_malloc(sizeof(cct_node_t*) * (capacity));
    size_t i = 0;
    for (; i < vector->size; ++i) {
      new_nodes[i] = vector->nodes[i];
    }
    vector->nodes = new_nodes;
  }
  vector->capacity = capacity;
}

void cct_node_vector_push_back(cct_node_vector_t* vector, cct_node_t* node) {
  if (vector->size == vector->capacity) {
    cct_node_vector_reserve(vector, vector->capacity * 2);
  }
  vector->nodes[vector->size] = node;
  vector->size++;
}

cct_node_t* cct_node_vector_get(cct_node_vector_t* vector, uint64_t index) {
  if (index < vector->size) {
    return vector->nodes[index];
  } else {
    // out of the bound
    return NULL;
  }
}
