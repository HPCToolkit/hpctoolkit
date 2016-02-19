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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "BalancedTree.h"


//*************************** Forward Declarations **************************

static void
BalancedTree_leftRotate(BalancedTree_t* tree, BalancedTreeNode_t* x);

static void
BalancedTree_rightRotate(BalancedTree_t* tree, BalancedTreeNode_t* y);

//***************************************************************************
// 
//***************************************************************************


void
BalancedTreeNode_init(BalancedTreeNode_t* x, 
		      void* key, BalancedTreeNode_t* parent)
{
  x->left = NULL;
  x->right = NULL;
  x->parent = parent;
  x->color = BalancedTreeColor_RED;

  x->key = key;
  // x->data -- client's responsibility
}


//***************************************************************************
// 
//***************************************************************************

void 
BalancedTree_init(BalancedTree_t* tree, 
		  BalancedTree_alloc_fn_t allocFn, size_t nodeDataSz)
{
  tree->root = NULL;
  tree->size = 0;

  tree->allocFn = allocFn;
  tree->nodeDataSz = nodeDataSz;

  QueuingRWLock_init(&tree->lock);
  spinlock_unlock(&tree->spinlock);
}


BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key,
		    QueuingRWLockLcl_t* locklcl)
{
  if (locklcl) {
    QueuingRWLock_lock(&tree->lock, locklcl, QueuingRWLockOp_write);
    //spinlock_lock(&tree->spinlock);
  }
 
  BalancedTreeNode_t* found = NULL;

  // ------------------------------------------------------------
  // empty tree
  // ------------------------------------------------------------
  if (!tree->root) {
    tree->root = BalancedTreeNode_alloc(tree->allocFn, tree->nodeDataSz);
    BalancedTreeNode_init(tree->root, key, NULL);

    tree->root->color = BalancedTreeColor_BLACK;
    tree->size = 1;
    found = tree->root;
    goto fini;
  }


  // ------------------------------------------------------------
  // find existing node or new location
  // ------------------------------------------------------------
  BalancedTreeNode_t* x = tree->root;
  BalancedTreeNode_t* x_parent = NULL;

  int pathDir = 0;
  while (x != NULL) {
    if (key == x->key) {
      found = x; // found!
      goto fini;
    }
    else {
      x_parent = x;
      if (key < x->key) {
	x = x->left;
	pathDir = -1;
      }
      else {
	x = x->right;
	pathDir = +1;
      }
    }
  }

  // insert (INVARIANT: pathDir is -1 or 1)
  if (pathDir < 0) {
    found = x = x_parent->left = 
      BalancedTreeNode_alloc(tree->allocFn, tree->nodeDataSz);
  }
  else {
    found = x = x_parent->right = 
      BalancedTreeNode_alloc(tree->allocFn, tree->nodeDataSz);
  }
  BalancedTreeNode_init(x, key, x_parent);
  tree->size++;


  // ------------------------------------------------------------
  // rebalance
  // ------------------------------------------------------------
  BalancedTreeNode_t* x_gparent = NULL;

  while (x != tree->root && x->parent->color == BalancedTreeColor_RED) {

    x_parent = x->parent;
    x_gparent = x_parent->parent;

    if (x_parent == x_gparent->left) {
      BalancedTreeNode_t* y = x_gparent->right;
      if (y && y->color == BalancedTreeColor_RED) {
	x_parent->color = BalancedTreeColor_BLACK;
	y->color = BalancedTreeColor_BLACK;
	x_gparent->color = BalancedTreeColor_RED;
	x = x_gparent;
      }
      else {
	if (x == x_parent->right) {
	  x = x_parent;
	  BalancedTree_leftRotate(tree, x);
	  x_parent = x->parent;
	  x_gparent = x_parent->parent;
	}
	x_parent->color = BalancedTreeColor_BLACK;
	x_gparent->color = BalancedTreeColor_RED;
	BalancedTree_rightRotate(tree, x_gparent);
      }
    }
    else {
      BalancedTreeNode_t* y = x_gparent->left;
      if (y && y->color == BalancedTreeColor_RED) {
	x_parent->color = BalancedTreeColor_BLACK;
	y->color = BalancedTreeColor_BLACK;
	x_gparent->color = BalancedTreeColor_RED;
	x = x_gparent;
      }
      else {
	if (x == x_parent->left) {
	  x = x_parent;
	  BalancedTree_rightRotate(tree, x);
	  x_parent = x->parent;
	  x_gparent = x_parent->parent;
	}
	x_parent->color = BalancedTreeColor_BLACK;
	x_gparent->color = BalancedTreeColor_RED;
	BalancedTree_leftRotate(tree, x_gparent);
      }
    }
  }

  tree->root->color = BalancedTreeColor_BLACK;

 fini:
  if (locklcl) {
    QueuingRWLock_unlock(&tree->lock, locklcl);
    //spinlock_unlock(&tree->spinlock);
  }
  return found;
}


//     y           x       |
//    / \         / \      |
//   x   c  <==  a   y     |
//  / \             / \    |
// a   b           b   c   |
static void
BalancedTree_leftRotate(BalancedTree_t* tree, BalancedTreeNode_t* x)
{
  // set y (INVARIANT: y != NULL)
  BalancedTreeNode_t* y = x->right;
  
  // move b to x's right subtree
  x->right = y->left;
  if (y->left != NULL) {
    y->left->parent = x;
  }
  
  // move y to x's old position
  y->parent = x->parent;
  if (x->parent == NULL) {
    tree->root = y;
  }
  else {
    if (x == x->parent->left) {
      x->parent->left = y;
    }
    else {
      x->parent->right = y;
    }
  }

  // move x to y's left
  y->left = x;
  x->parent = y;
}


//     y           x       |
//    / \         / \      |
//   x   c  ==>  a   y     |
//  / \             / \    |
// a   b           b   c   |
static void
BalancedTree_rightRotate(BalancedTree_t* tree, BalancedTreeNode_t* y)
{
  // set x (INVARIANT: x != NULL)
  BalancedTreeNode_t* x = y->left;
  
  // move b to y's left subtree
  y->left = x->right;
  if (x->right != NULL) {
    x->right->parent = y;
  }

  // move x to y's old position
  x->parent = y->parent;
  if (y->parent == NULL) {
    tree->root = x;
  }
  else {
    if (y == y->parent->left) {
      y->parent->left = x;
    }
    else {
      y->parent->right = x;
    }
  }

  // move y to x's left
  x->right = y;
  y->parent = x;
}


#if 0
static void 
BalancedTree_foreach_rec(BalancedTreeNode_t*, 
			 BalancedTree_foreach_func, void*);

void
BalancedTree_foreach(BalancedTree_t* tree,
		     BalancedTree_foreach_func func, void* data)
{
  BalancedTree_foreach_rec(tree->root, func, data);
}


static void
BalancedTree_foreach_rec(BalancedTreeNode_t* node,
			 BalancedTree_foreach_func func, void* data)
{
  if (node != NULL) {
    BalancedTree_foreach_rec(node->left, func, data);

    func(node, data);

    BalancedTree_foreach_rec(node->right, func, data);
  }
}
#endif
