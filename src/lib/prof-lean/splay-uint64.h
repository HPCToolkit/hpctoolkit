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

//******************************************************************************
// File: splay-tree.h
//
// Description:
//   extensible splay tree implementation that uses 64-bit unsigned keys
//******************************************************************************

#ifndef splay_uint64_h
#define splay_uint64_h

#include <stdbool.h>
#include <stdint.h>

typedef struct splay_uint64_node_t {
  struct splay_uint64_node_t* left;
  struct splay_uint64_node_t* right;
  uint64_t key;
} splay_uint64_node_t;
;

typedef enum splay_order_t { splay_inorder = 1, splay_allorder = 2 } splay_order_t;

typedef enum splay_visit_t {
  splay_preorder_visit = 1,
  splay_inorder_visit = 2,
  splay_postorder_visit = 3
} splay_visit_t;

typedef void (*splay_fn_t)(splay_uint64_node_t* node, splay_visit_t order, void* arg);

#define splay_macro_body_ignore(x) ;
#define splay_macro_body_show(x)   x

// declare typed interface functions
#define typed_splay_declare(type) typed_splay(type, splay_macro_body_ignore)

// implementation of typed interface functions
#define typed_splay_impl(type) typed_splay(type, splay_macro_body_show)

// routine name for a splay operation
#define splay_op(op) splay_uint64##_##op

#define typed_splay_node(type) type##_##splay_uint64_node_t

// routine name for a typed splay operation
#define typed_splay_op(type, op) type##_##op

// insert routine name for a typed splay
#define typed_splay_splay(type) typed_splay_op(type, splay)

// insert routine name for a typed splay
#define typed_splay_insert(type) typed_splay_op(type, insert)

// lookup routine name for a typed splay
#define typed_splay_lookup(type) typed_splay_op(type, lookup)

// delete routine name for a typed splay
#define typed_splay_delete(type) typed_splay_op(type, delete)

// forall routine name for a typed splay
#define typed_splay_forall(type) typed_splay_op(type, forall)

// count routine name for a typed splay
#define typed_splay_count(type) typed_splay_op(type, count)

// define typed wrappers for a splay type
#define typed_splay(type, macro)                                                               \
  static bool typed_splay_insert(type)(                                                        \
      typed_splay_node(type) * *root, typed_splay_node(type) * node) macro({                   \
    return splay_op(insert)((splay_uint64_node_t**)root, (splay_uint64_node_t*)node);          \
  })                                                                                           \
                                                                                               \
          static typed_splay_node(type)                                                        \
      * typed_splay_lookup(type)(typed_splay_node(type) * *root, uint64_t key) macro({         \
          typed_splay_node(type)* result =                                                     \
              (typed_splay_node(type)*)splay_op(lookup)((splay_uint64_node_t**)root, key);     \
          return result;                                                                       \
        })                                                                                     \
                                                                                               \
          static typed_splay_node(type)* __attribute__((unused)) /* prevent unused warnings */ \
          typed_splay_delete(type)(typed_splay_node(type) * *root, uint64_t key) macro({       \
            typed_splay_node(type)* result =                                                   \
                (typed_splay_node(type)*)splay_op(delete)((splay_uint64_node_t**)root, key);   \
            return result;                                                                     \
          })                                                                                   \
                                                                                               \
              static void __attribute__((unused)) /* prevent unused warnings */                \
              typed_splay_forall(type)(                                                        \
                  typed_splay_node(type) * root, splay_order_t order,                          \
                  void (*fn)(typed_splay_node(type)*, splay_visit_t visit, void*), void* arg)  \
                  macro({                                                                      \
                    splay_op(forall)((splay_uint64_node_t*)root, order, (splay_fn_t)fn, arg);  \
                  }) static uint64_t __attribute__((unused)) /* prevent unused warnings */     \
                  typed_splay_count(type)(typed_splay_node(type) * node)                       \
                      macro({ return splay_op(count)((splay_uint64_node_t*)node); })

//------------------------------------------------------------------------------
// inserts node at root of tree
//
// returns false if node->key already present in tree and node not inserted
//------------------------------------------------------------------------------
bool splay_uint64_insert(splay_uint64_node_t** root, splay_uint64_node_t* node);

//------------------------------------------------------------------------------
// look up key in tree.
//
// if key is found: node with key will be at root; returns pointer to node
//
// returns NULL if key not found
//------------------------------------------------------------------------------
splay_uint64_node_t* splay_uint64_lookup(splay_uint64_node_t** root, uint64_t key);

//------------------------------------------------------------------------------
// returns deleted node with matching key, or NULL if key is not found
//------------------------------------------------------------------------------
splay_uint64_node_t* splay_uint64_delete(splay_uint64_node_t** root, uint64_t key);

//------------------------------------------------------------------------------
// iterates over all nodes and calls fn(node, visit_type, arg)
//
// for inorder traversal:
//   fn invoked once per node with visit type splay_inorder_visit
//
// for allorder traversal:
//   fn invoked three times per node
//     first with splay_preorder_visit before visiting left child
//     second with splay_inorder_visit after visiting left child
//     third with splay_postorder_visit after visiting right child
//------------------------------------------------------------------------------
void splay_uint64_forall(splay_uint64_node_t* root, splay_order_t order, splay_fn_t fn, void* arg);

//------------------------------------------------------------------------------
// return an inclusive count of the number of nodes in the subtree
// rooted at node
//------------------------------------------------------------------------------
uint64_t splay_uint64_count(splay_uint64_node_t* node);

#endif
