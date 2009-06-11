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

#ifndef hpcrun_binTree_h
#define hpcrun_binTree_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <utilities/spinlock.h>

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

  spinlock_t lock;
  
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
BalancedTree_find(BalancedTree_t* tree, void* key)
{
  BalancedTreeNode_t* x = tree->root;

  while (x != NULL) {
    if (key == x->key) {
      return x; // found!
    }
    else if (key < x->key) {
      x = x->left;
    }
    else {
      x = x->right;
    }
  }
  
  return NULL;
}


// BalancedTree_insert: Insert `key' into `tree' (if it does not
// already exist).  Returns the node containing the key, which may
// have already existed or be newly allocated (using
// (hpcrun_malloc()).
BalancedTreeNode_t*
BalancedTree_insert(BalancedTree_t* tree, void* key, bool doAtomic);


#if 0
typedef void (*BalancedTree_foreach_func) (BalancedTreeNode_t*, void*);
void BalancedTree_foreach(BalancedTree_t*, BalancedTree_foreach_func, void*);
#endif


#endif /* BalancedTree_h */
