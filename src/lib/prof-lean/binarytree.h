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
//   Define an API for binary trees that supports 
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

#ifndef __binarytree_h__
#define __binarytree_h__

//******************************************************************************
// macros
//******************************************************************************

#define BINARYTREE_FIND(root, cmp, val) \
  binarytree_find((binarytree_node_t *)root, (compare_fn) cmp, val)

#define BINARYTREE_REBALANCE(root) \
  binarytree_rebalance((binarytree_node_t *)root)



//******************************************************************************
// types 
//******************************************************************************

typedef struct binarytree_node_s {
  struct binarytree_node_s *left;
  struct binarytree_node_s *right;
} binarytree_node_t;


// compare_fn will compare an arbitrary value with the contents of a node.
// a node might represent a value or an interval.
//
// compare_fn returns:
//   0 if val matches value(node)
//  <0 if val < value(node)
//  >0 if val > value(node)
typedef int (*compare_fn)(binarytree_node_t *node, void *val);



//******************************************************************************
// interface operations
//******************************************************************************

// perform bulk rebalancing by gathering nodes into a vector and
// rebuilding the tree from scratch using the same nodes.
binarytree_node_t *binarytree_rebalance(binarytree_node_t *root);

// use compare_fn to find a matching node in a binary tree. NULL is returned
// if no match is found. 
binarytree_node_t *binarytree_find(binarytree_node_t *root,
                                   compare_fn fn, void *val);

#endif
