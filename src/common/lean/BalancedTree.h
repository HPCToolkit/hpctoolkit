// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Balanced Trees (Red-black) in the style of CLR
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Based on code by Nathan Froyd.
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef prof_lean_BalancedTree_h
#define prof_lean_BalancedTree_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************


#include "pfq-rwlock.h"


//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
//
//***************************************************************************

typedef void* (*BalancedTree_alloc_fn_t)(size_t);

typedef enum {

  BalancedTreeColor_RED,
  BalancedTreeColor_BLACK

} BalancedTreeColor_t;


typedef struct BalancedTreeNode
{
  // node structure
  struct BalancedTreeNode* parent;
  struct BalancedTreeNode* left;
  struct BalancedTreeNode* right;
  BalancedTreeColor_t color;

  // node data
  void* key;
  void* data;

} BalancedTreeNode_t;



static inline BalancedTreeNode_t*
BalancedTreeNode_alloc(BalancedTree_alloc_fn_t alloc, size_t dataSz)
{
  BalancedTreeNode_t* x = (BalancedTreeNode_t*) alloc(sizeof(BalancedTreeNode_t));
  x->data = alloc(dataSz);
  return x;
}


void
BalancedTreeNode_init(BalancedTreeNode_t* x,
                      void* key, BalancedTreeNode_t* parent);


//***************************************************************************
//
//***************************************************************************

typedef struct BalancedTree
{
  BalancedTreeNode_t* root;
  unsigned int size;

  BalancedTree_alloc_fn_t allocFn;
  size_t nodeDataSz; // size of BalancedTreeNode_t.data

  pfq_rwlock_t rwlock;
} BalancedTree_t;


void
BalancedTree_init(BalancedTree_t* tree,
                  BalancedTree_alloc_fn_t allocFn, size_t nodeDataSz);


static inline unsigned int
BalancedTree_size(BalancedTree_t* tree)
{
  return tree->size;
}


static inline BalancedTreeNode_t*
BalancedTree_find(BalancedTree_t* tree, void* key)
{
  pfq_rwlock_read_lock(&tree->rwlock);
  BalancedTreeNode_t* curr = tree->root;
  while (curr != NULL && curr->key != key) {
    if (key < curr->key)
      curr = curr->left;
    else
      curr = curr->right;
  }
  pfq_rwlock_read_unlock(&tree->rwlock);
  return curr;
}


// BalancedTree_insert: Insert `key' into `tree' (if it does not
// already exist).  Returns the node containing the key, which may
// have already existed or be newly allocated.
BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key);


#endif /* prof_lean_BalancedTree_h */
