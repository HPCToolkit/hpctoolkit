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
// Copyright ((c)) 2002-2016, Rice University
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

#include <include/uint.h>

#include "QueuingRWLock.h"
#include "spinlock.h"


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
  BalancedTreeNode_t* x = alloc(sizeof(BalancedTreeNode_t));
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
  uint size;

  BalancedTree_alloc_fn_t allocFn;
  size_t nodeDataSz; // size of BalancedTreeNode_t.data

  QueuingRWLock_t lock;
  spinlock_t spinlock;
  
} BalancedTree_t;


void 
BalancedTree_init(BalancedTree_t* tree, 
		  BalancedTree_alloc_fn_t allocFn, size_t nodeDataSz);


static inline uint
BalancedTree_size(BalancedTree_t* tree)
{
  return tree->size;
}


static inline BalancedTreeNode_t*
BalancedTree_find(BalancedTree_t* tree, void* key, QueuingRWLockLcl_t* locklcl)
{
  if (locklcl) {
    QueuingRWLock_lock(&tree->lock, locklcl, QueuingRWLockOp_read);
    //while (spinlock_is_locked(&tree->spinlock));
  }

  BalancedTreeNode_t* found = NULL;

  BalancedTreeNode_t* x = tree->root;
  while (x != NULL) {
    if (key == x->key) {
      found = x; // found!
      goto fini;
    }
    else if (key < x->key) {
      x = x->left;
    }
    else {
      x = x->right;
    }
  }

 fini:
  if (locklcl) {
    QueuingRWLock_unlock(&tree->lock, locklcl);
  }
  return found;
}


// BalancedTree_insert: Insert `key' into `tree' (if it does not
// already exist).  Returns the node containing the key, which may
// have already existed or be newly allocated.
BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key, 
		    QueuingRWLockLcl_t* locklcl);


#if 0
typedef void (*BalancedTree_foreach_func) (BalancedTreeNode_t*, void*);
void BalancedTree_foreach(BalancedTree_t*, BalancedTree_foreach_func, void*);
#endif


#endif /* prof_lean_BalancedTree_h */
