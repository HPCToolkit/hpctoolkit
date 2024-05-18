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

  pfq_rwlock_init(&tree->rwlock);
}


BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key)
{
  pfq_rwlock_node_t locklcl;
  pfq_rwlock_write_lock(&tree->rwlock, &locklcl);

  BalancedTreeNode_t* x = tree->root;
  BalancedTreeNode_t* x_parent = NULL;

  // ------------------------------------------------------------
  // find existing node or new location
  // ------------------------------------------------------------

  while (x != NULL && x->key != key) {
      x_parent = x;
      if (key < x->key)
        x = x->left;
      else
        x = x->right;
  }
  BalancedTreeNode_t* found;
  if (x != NULL)
    found = x;
  else {
    found = x =
      BalancedTreeNode_alloc(tree->allocFn, tree->nodeDataSz);
    BalancedTreeNode_init(x, key, x_parent);
    tree->size++;
    // ------------------------------------------------------------
    // empty tree
    // ------------------------------------------------------------
    if (!tree->root)
      tree->root = x;


    // ------------------------------------------------------------
    // rebalance
    // ------------------------------------------------------------

    while (x != tree->root && x->parent->color == BalancedTreeColor_RED) {

      x_parent = x->parent;
      BalancedTreeNode_t* x_gparent = x_gparent = x_parent->parent;

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
  }

  pfq_rwlock_write_unlock(&tree->rwlock, &locklcl);
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
