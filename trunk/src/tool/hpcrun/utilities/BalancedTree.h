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

#ifndef hpcrun_BalancedTree_h
#define hpcrun_BalancedTree_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof-lean/QueuingRWLock.h>
#include <lib/prof-lean/spinlock.h>


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


#endif /* hpcrun_BalancedTree_h */
