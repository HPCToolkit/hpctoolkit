// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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
// system includes
//******************************************************************************

#include <assert.h>
#include <string.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "cupti-cct-trace-map.h"

#define DEBUG
#ifdef DEBUG
#define TRACE_MAP_MSG(...) TMSG(__VA_ARGS__)
#else
#define TRACE_MAP_MSG(...)
#endif

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct splay_cct_trace_node_t {
  struct splay_cct_trace_node_t *left;
  struct splay_cct_trace_node_t *right;
  cct_trace_key_t key;
} splay_cct_trace_node_t;


typedef enum splay_order_t {
  splay_inorder = 1,
  splay_allorder = 2
} splay_order_t;


typedef enum splay_visit_t {
  splay_preorder_visit = 1,
  splay_inorder_visit = 2,
  splay_postorder_visit = 3
} splay_visit_t;


typedef void (*splay_fn_t)
(
 splay_cct_trace_node_t *node,
 splay_visit_t order,
 void *arg
);


static bool cct_trace_cmp_lt(cct_trace_key_t left, cct_trace_key_t right);
static bool cct_trace_cmp_gt(cct_trace_key_t left, cct_trace_key_t right);

#define cct_trace_builtin_lt(a, b) (cct_trace_cmp_lt(a, b))
#define cct_trace_builtin_gt(a, b) (cct_trace_cmp_gt(a, b))

#define CCT_TRACE_SPLAY_TREE(type, root, key, value, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, value, value, left, right, cct_trace_builtin_lt, cct_trace_builtin_gt)

//*****************************************************************************
// macros 
//*****************************************************************************

#define splay_macro_body_ignore(x) ;
#define splay_macro_body_show(x) x

// declare typed interface functions 
#define typed_splay_declare(type)		\
  typed_splay(type, splay_macro_body_ignore)

// implementation of typed interface functions 
#define typed_splay_impl(type)			\
  typed_splay(type, splay_macro_body_show)

// routine name for a splay operation
#define splay_op(op) \
  splay_cct_trace ## _ ## op

#define typed_splay_node(type) \
  type ## _ ## splay_cct_trace_node_t

// routine name for a typed splay operation
#define typed_splay_op(type, op) \
  type ## _  ## op

// insert routine name for a typed splay 
#define typed_splay_splay(type) \
  typed_splay_op(type, splay)

// insert routine name for a typed splay 
#define typed_splay_insert(type) \
  typed_splay_op(type, insert)

// lookup routine name for a typed splay 
#define typed_splay_lookup(type) \
  typed_splay_op(type, lookup)

// delete routine name for a typed splay 
#define typed_splay_delete(type) \
  typed_splay_op(type, delete)

// forall routine name for a typed splay 
#define typed_splay_forall(type) \
  typed_splay_op(type, forall)

// count routine name for a typed splay 
#define typed_splay_count(type) \
  typed_splay_op(type, count)

// define typed wrappers for a splay type 
#define typed_splay(type, macro) \
  static bool \
  typed_splay_insert(type) \
  (typed_splay_node(type) **root, typed_splay_node(type) *node)	\
  macro({ \
    return splay_op(insert)((splay_cct_trace_node_t **) root, (splay_cct_trace_node_t *) node); \
  }) \
\
  static typed_splay_node(type) * \
  typed_splay_lookup(type) \
  (typed_splay_node(type) **root, cct_trace_key_t key) \
  macro({ \
    typed_splay_node(type) *result = (typed_splay_node(type) *) \
      splay_op(lookup)((splay_cct_trace_node_t **) root, key); \
    return result; \
  }) \
\
  static typed_splay_node(type) * \
  typed_splay_delete(type) \
  (typed_splay_node(type) **root, cct_trace_key_t key) \
  macro({ \
    typed_splay_node(type) *result = (typed_splay_node(type) *) \
      splay_op(delete)((splay_cct_trace_node_t **) root, key); \
    return result; \
  }) 

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_cct_trace_alloc_helper		\
  ((splay_cct_trace_node_t **) free_list, sizeof(splay_node_type))	

#define typed_splay_free(free_list, node)			\
  splay_cct_trace_free_helper					\
  ((splay_cct_trace_node_t **) free_list,				\
   (splay_cct_trace_node_t *) node)

//******************************************************************************
// interface operations
//******************************************************************************

static bool
cct_trace_cmp_gt
(
 cct_trace_key_t left,
 cct_trace_key_t right
)
{
  if (left.cct1 > right.cct1) {
    return true;
  } else if (left.cct1 == right.cct1) {
    if (left.cct2 > right.cct2) {
      return true;
    }
  }
  return false;
}


static bool
cct_trace_cmp_lt
(
 cct_trace_key_t left,
 cct_trace_key_t right
)
{
  if (left.cct1 < right.cct1) {
    return true;
  } else if (left.cct1 == right.cct1) {
    if (left.cct2 < right.cct2) {
      return true;
    }
  }
  return false;
}


static bool
cct_trace_cmp_eq
(
 cct_trace_key_t left,
 cct_trace_key_t right
)
{
  return left.cct1 == right.cct1 && left.cct2 == right.cct2;
}


static splay_cct_trace_node_t *
splay_cct_trace_splay
(
 splay_cct_trace_node_t *root,
 cct_trace_key_t splay_key
)
{
  CCT_TRACE_SPLAY_TREE(splay_cct_trace_node_t, root, splay_key, key, left, right);

  return root;
}


static void
splay_delete_root
(
 splay_cct_trace_node_t **root
)
{
  splay_cct_trace_node_t *map_root = *root;

  if (map_root->left == NULL) {
    map_root = map_root->right;
  } else {
    map_root->left = splay_cct_trace_splay(map_root->left, map_root->key);
    map_root->left->right = map_root->right;
    map_root = map_root->left;
  }

  // detach deleted node from others
  (*root)->left = (*root)->right = NULL; 

  // set new root
  *root = map_root; 
}


static splay_cct_trace_node_t *
splay_cct_trace_delete
(
 splay_cct_trace_node_t **root,
 cct_trace_key_t key
)
{
  splay_cct_trace_node_t *removed = NULL;
  
  if (*root) {
    *root = splay_cct_trace_splay(*root, key);

    if (cct_trace_cmp_eq((*root)->key, key)) {
      removed = *root;
      splay_delete_root(root);
    }
  }

  return removed;
}

static bool
splay_cct_trace_insert
(
 splay_cct_trace_node_t **root,
 splay_cct_trace_node_t *node
)
{
  node->left = node->right = NULL;

  if (*root != NULL) {
    *root = splay_cct_trace_splay(*root, node->key);
    
    if (cct_trace_cmp_lt(node->key, (*root)->key)) {
      node->left = (*root)->left;
      node->right = *root;
      (*root)->left = NULL;
    } else if (cct_trace_cmp_gt(node->key, (*root)->key)) {
      node->left = *root;
      node->right = (*root)->right;
      (*root)->right = NULL;
    } else {
      // key already present in the tree
      return true; // insert failed 
    }
  } 

  *root = node;

  return true; // insert succeeded 
}


static splay_cct_trace_node_t *
splay_cct_trace_lookup
(
 splay_cct_trace_node_t **root,
 cct_trace_key_t key
)
{
  splay_cct_trace_node_t *result = 0;

  *root = splay_cct_trace_splay(*root, key);

  if (*root && cct_trace_cmp_eq((*root)->key, key)) {
    result = *root;
  }

  return result;
}


static splay_cct_trace_node_t *
splay_cct_trace_alloc_helper
(
 splay_cct_trace_node_t **free_list, 
 size_t size
)
{
  splay_cct_trace_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (splay_cct_trace_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


static void
splay_cct_trace_free_helper
(
 splay_cct_trace_node_t **free_list, 
 splay_cct_trace_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}

//******************************************************************************
// private operations
//******************************************************************************

#define st_insert				\
  typed_splay_insert(cct_trace)

#define st_lookup				\
  typed_splay_lookup(cct_trace)

#define st_delete				\
  typed_splay_delete(cct_trace)

#define st_forall				\
  typed_splay_forall(cct_trace)

#define st_count				\
  typed_splay_count(cct_trace)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(cct_trace))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(cct_trace) cupti_cct_trace_map_entry_t

struct cupti_cct_trace_map_entry_s {
  struct cupti_cct_trace_map_entry_s *left;
  struct cupti_cct_trace_map_entry_s *right;
  cct_trace_key_t key;
  cupti_cct_trace_node_t *trace_node;
};

static __thread cupti_cct_trace_map_entry_t *map_root = NULL;
static __thread cupti_cct_trace_map_entry_t *free_list = NULL;

typed_splay_impl(cct_trace)


static cupti_cct_trace_map_entry_t *
cupti_cct_trace_map_new
(
 cct_trace_key_t key,
 cupti_cct_trace_node_t *trace_node
)
{
  cupti_cct_trace_map_entry_t *entry = st_alloc(&free_list);
  
  entry->key = key;
  entry->trace_node = trace_node;

  return entry;
}


cupti_cct_trace_map_entry_t *
cupti_cct_trace_map_lookup
(
 cct_trace_key_t key
)
{
  cupti_cct_trace_map_entry_t *entry = st_lookup(&map_root, key);

  TRACE_MAP_MSG(CUPTI_CCT_TRACE, "Trace map lookup (cct1: %p, cct1: %p)->(entry: %p)", key.cct1, key.cct2, entry);
  
  return entry;
} 


void
cupti_cct_trace_map_insert
(
 cct_trace_key_t key,
 cupti_cct_trace_node_t *trace_node
)
{
  cupti_cct_trace_map_entry_t *entry = st_lookup(&map_root, key);

  if (entry == NULL) {
    entry = cupti_cct_trace_map_new(key, trace_node);
    st_insert(&map_root, entry);
  }

  TRACE_MAP_MSG(CUPTI_CCT_TRACE, "Trace map insert (cct1: %p, cct1: %p)->(trace_node: %p)", key.cct1, key.cct2, trace_node);
}


void
cupti_cct_trace_map_delete
(
 cct_trace_key_t key
)
{
  cupti_cct_trace_map_entry_t *entry = st_delete(&map_root, key);

  if (entry != NULL) {
    st_free(&free_list, entry);
  }
  
  TRACE_MAP_MSG(CUPTI_CCT_TRACE, "Trace map delete (cct1: %p, cct1: %p)->(trace_node: %p)", key.cct1, key.cct2, entry->trace_node);
}


cupti_cct_trace_node_t *
cupti_cct_trace_map_entry_cct_trace_node_get
(
 cupti_cct_trace_map_entry_t *entry
)
{
  return entry->trace_node;
}
