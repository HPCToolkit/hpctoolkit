// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
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

#include <memory/mem.h>

#include <utilities/spinlock.h>


//*************************** Forward Declarations **************************

// tallent: Since I found a bug in the insertion code, turn off
// balancing for now
#define PLEASE_BALANCE_THANK_YOU 0

void 
BalancedTree_right_rotate(BalancedTree_t*, BalancedTreeNode_t*);

void 
BalancedTree_left_rotate(BalancedTree_t*, BalancedTreeNode_t*);

//***************************************************************************
// 
//***************************************************************************

BalancedTreeNode_t*
BalancedTreeNode_create(void* key, BalancedTreeNode_t* parent)
{
  BalancedTreeNode_t* x = csprof_malloc(sizeof(BalancedTreeNode_t));

  x->left = NULL;
  x->right = NULL;
  x->parent = parent;
  x->color = BalancedTreeColor_RED;
  x->key = key;

  x->count = 0;

  return x;
}


//***************************************************************************
// 
//***************************************************************************

BalancedTree_t*
BalancedTree_create()
{
  BalancedTree_t* tree = csprof_malloc(sizeof(BalancedTree_t));
  BalancedTree_init(tree);
  return tree;
}


void 
BalancedTree_init(BalancedTree_t* tree)
{
  tree->root = NULL;
  tree->size = 0;
}


BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key, bool doAtomic)
{
  if (doAtomic) {
    spinlock_lock(tree->lock);
  }

  BalancedTreeNode_t* theNode = NULL;

  // ------------------------------------------------------------
  // empty tree
  // ------------------------------------------------------------
  if (!tree->root) {
    tree->root = BalancedTreeNode_create(key, NULL);
    tree->size = 1;
    theNode = tree->root;
    goto fini;
  }


  // ------------------------------------------------------------
  // find existing node or new location
  // ------------------------------------------------------------
  BalancedTreeNode_t* curr = tree->root;
  BalancedTreeNode_t* parent = NULL;

  int pathDir = 0;
  while (curr != NULL) {
    if (key == curr->key) {
      theNode = curr; // found!
      goto fini;
    }
    else {
      parent = curr;
      if (key < curr->key) {
	curr = curr->left;
	pathDir = -1;
      }
      else {
	curr = curr->right;
	pathDir = +1;
      }
    }
  }

  // insert (INVARIANT: pathDir is -1 or 1)
  if (pathDir < 0) {
    theNode = curr = parent->left = BalancedTreeNode_create(key, parent);
  }
  else {
    theNode = curr = parent->right = BalancedTreeNode_create(key, parent);
  }
  tree->size++;


  // ------------------------------------------------------------
  // rebalance
  // ------------------------------------------------------------
#if (PLEASE_BALANCE_THANK_YOU)
  BalancedTreeNode_t* gparent = NULL;

  while ((parent = curr->parent) != NULL
	 && (gparent = parent->parent) != NULL
	 && curr->parent->color == BalancedTreeColor_RED) {
    if (parent == gparent->left) {
      if (gparent->right != NULL 
	  && gparent->right->color == BalancedTreeColor_RED) {
	parent->color = BalancedTreeColor_BLACK;
	gparent->right->color = BalancedTreeColor_BLACK;
	gparent->color = BalancedTreeColor_RED;
	curr = gparent;
      }
      else {
	if (curr == parent->right) {
	  curr = parent;
	  BalancedTree_left_rotate(tree, curr);
	  parent = curr->parent;
	}
	parent->color = BalancedTreeColor_BLACK;
	if ((gparent = parent->parent) != NULL) {
	  gparent->color = BalancedTreeColor_RED;
	  BalancedTree_right_rotate(tree, gparent);
	}
      }
    }
    /* mirror case */
    else {
      if (gparent->left != NULL && gparent->left->color == BalancedTreeColor_RED) {
	parent->color = BalancedTreeColor_BLACK;
	gparent->left->color = BalancedTreeColor_BLACK;
	gparent->color = BalancedTreeColor_RED;
	curr = gparent;
      }
      else {
	if (curr == parent->left) {
	  curr = parent;
	  BalancedTree_right_rotate(tree, curr);
	  parent = curr->parent;
	}
	parent->color = BalancedTreeColor_BLACK;
	if ((gparent = parent->parent) != NULL) {
	  gparent->color = BalancedTreeColor_RED;
	  BalancedTree_left_rotate(tree, gparent);
	}
      }
    }
  }
  if (curr->parent == NULL) {
    tree->root = curr;
  }
  tree->root->color = BalancedTreeColor_BLACK;
#endif // PLEASE_BALANCE_THANK_YOU

 fini:
  if (doAtomic) {
    spinlock_unlock(tree->lock);
  }
  return newNode;
}


#if (PLEASE_BALANCE_THANK_YOU)
/* Rotate the right child of parent upwards */
static void
BalancedTree_left_rotate(BalancedTree_t* tree, BalancedTreeNode_t* parent)
{
  BalancedTreeNode_t *curr, *gparent;

  curr = RIGHT(parent);
  gparent = parent->parent;

  if (curr != NULL) {
    parent->right = LEFT(curr);
    if (LEFT(curr) != NULL) {
      LEFT(curr)->parent = parent;
    }

    curr->parent = parent->parent;
    if (gparent == NULL) {
      tree->root = curr;
    }
    else {
      int val = (parent == LEFT(gparent));

      gparent->children[val] = curr;
    }
    LEFT(curr) = parent;
    parent->parent = curr;
  }
}


/* Rotate the left child of parent upwards */
static void
BalancedTree_right_rotate(BalancedTree_t* tree, BalancedTreeNode_t* parent)
{
  BalancedTreeNode_t *curr, *gparent;

  curr = LEFT(parent);
  gparent = parent->parent;

  if (curr != NULL) {
    parent->left = RIGHT(curr);
    if (RIGHT(curr) != NULL) {
      RIGHT(curr)->parent = parent;
    }
    curr->parent = parent->parent;
    if (gparent == NULL) {
      tree->root = curr;
    }
    else {
      int val = (parent == LEFT(gparent));

      gparent->children[val] = curr;
    }
    RIGHT(curr) = parent;
    parent->parent = curr;
  }
}
#endif // PLEASE_BALANCE_THANK_YOU


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
