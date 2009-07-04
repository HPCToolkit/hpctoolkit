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


//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// 
//***************************************************************************

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

  // interface to the node data [tallent: could be a callback to node data]
  void* key;

  // node data: [tallent:FIXME: should be opaque node data (a void*)]
  uint64_t idleness;
  void*    cct_node;
  bool isBlockingWork;
  bool isLocked;

} BalancedTreeNode_t;


BalancedTreeNode_t*
BalancedTreeNode_create(void* key, BalancedTreeNode_t* parent);


//***************************************************************************
// 
//***************************************************************************

typedef struct BalancedTree
{
  BalancedTreeNode_t* root;
  uint size;

  QueuingRWLock_t lock;
  
} BalancedTree_t;


BalancedTree_t*
BalancedTree_create();

void 
BalancedTree_init(BalancedTree_t* tree);


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
// have already existed or be newly allocated (using
// (hpcrun_malloc()).
BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key, 
		    QueuingRWLockLcl_t* locklcl);


#if 0
typedef void (*BalancedTree_foreach_func) (BalancedTreeNode_t*, void*);
void BalancedTree_foreach(BalancedTree_t*, BalancedTree_foreach_func, void*);
#endif


#endif /* hpcrun_BalancedTree_h */
