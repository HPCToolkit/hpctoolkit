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
// Copyright ((c)) 2002-2015, Rice University
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
//
// File:
//   $HeadURL$
//
// Purpose:
//   Implement an API for binary trees that supports 
//   (1) building a balanced binary tree from an arbitrary imbalanced tree 
//       (e.g., a list), and 
//   (2) searching a binary tree for a matching node  
//
//   The binarytree_node_t data type is meant to be used as a prefix to 
//   other structures so that this code can be used to manipulate arbitrary
//   tree structures.  Macros support the (unsafe) up and down casting needed 
//   to use the API on derived structures.
//
//******************************************************************************



//******************************************************************************
// local include files 
//******************************************************************************

#include "binarytree.h"



//******************************************************************************
// macros 
//******************************************************************************

#ifndef NULL
#define NULL 0
#endif



//******************************************************************************
// private operations
//******************************************************************************

static unsigned int
tree_count(binarytree_node_t *node) 
{
  return node ? (tree_count(node->left) + tree_count(node->right) + 1) : 0;
}


// perform an inorder traversal and collect nodes into a vector
static void
tree_to_vector_helper
(binarytree_node_t *nvec[], 
 binarytree_node_t *node, 
 int *count)
{
  if (node) {
    // add nodes in left subtree to vector
    tree_to_vector_helper(nvec, node->left,  count);
    
    // add subtree root to vector
    nvec[(*count)++] = node;
    
    // add nodes in right subtree to vector
    tree_to_vector_helper(nvec, node->right, count);
  }
}


static void
tree_to_vector
(binarytree_node_t *nvec[], 
 binarytree_node_t *root) 
{
  int count = 0;
  tree_to_vector_helper(nvec, root, &count);
}


// post-condition: 0 <= |depth(left subtree) - depth(right subtree)| <= 1 
static binarytree_node_t *
vector_to_binarytree(binarytree_node_t *nvec[], int l, int u)
{
  // the midpoint of the subvector is the root of the subtree
  int mid = ((u - l) >> 1) + l;
  binarytree_node_t *root = nvec[mid];

  // build left subtree, if any, from nodes to the left of the midpoint
  root->left  = (mid != l) ? vector_to_binarytree(nvec, l, mid - 1) : NULL;

  // build right subtree, if any, from nodes to the right of the midpoint
  root->right = (mid != u) ? vector_to_binarytree(nvec, mid + 1, u) : NULL;

  return root;
}



//******************************************************************************
// interface operations
//******************************************************************************

binarytree_node_t *
binarytree_rebalance(binarytree_node_t *root)
{
  int n = tree_count(root);

  // collect tree nodes in order into a vector
  binarytree_node_t *vec[n];  
  tree_to_vector(vec, root);

  // construct a balanced binary tree from an ordered vector of nodes
  return vector_to_binarytree(vec, 0, n - 1);
}


binarytree_node_t *
binarytree_find
(binarytree_node_t *subtree_root,
 compare_fn matches,
 void *val)
{
  while (subtree_root) {
    int cmp_status = matches(subtree_root, val);
  
    if (cmp_status == 0) return subtree_root; // subtree_root matches val
  
    // determine which subtree to search
    subtree_root = (cmp_status < 0) ? subtree_root->left : subtree_root->right;
  }
  return NULL;
}
