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
// Copyright ((c)) 2002-2017, Rice University
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
// global include files
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "generic_val.h"
#include "mem_manager.h"

//******************************************************************************
// macros
//******************************************************************************

#define MAX_TREE_STR 65536
#define MAX_INDENTS  512
#define OPAQUE_TYPE 0

//******************************************************************************
// abstract data type
//******************************************************************************

#if OPAQUE_TYPE

// opaque type not supported by gcc 4.4.*
typedef struct binarytree_s binarytree_t;

#else

typedef struct binarytree_s {
  struct binarytree_s *left;
  struct binarytree_s *right;
  void* val;
} binarytree_t;

#endif

// constructors
binarytree_t *
binarytree_new(void* value, binarytree_t *left, binarytree_t *right,
	mem_alloc m_alloc);

// destructor
void binarytree_del(binarytree_t **root, mem_free m_free);

/*
 * Gettors
 */
// return the value at the root
// pre-condition: tree != NULL
void*
binarytree_rootval(binarytree_t *tree);

// pre-condition: tree != NULL
binarytree_t*
binarytree_leftsubtree(binarytree_t *tree);

// pre-condition: tree != NULL
binarytree_t*
binarytree_rightsubtree(binarytree_t *tree);

/*
 * Settors
 */
void
binarytree_set_rootval(
	binarytree_t *tree,
	void* rootval);

void
binarytree_set_leftsubtree(
	binarytree_t *tree,
	binarytree_t* subtree);

void
binarytree_set_rightsubtree(
	binarytree_t *tree,
	binarytree_t* subtree);

// count the number of nodes in the binary tree.
int
binarytree_count(binarytree_t *node);

// linearize a binary tree in order traversal into a vector
void
binarytree_to_vector(binarytree_t *nvec[], binarytree_t *tree);

// construct a balanced binary tree from a vector of nodes.
// post-condition: 0 <= |height(left subtree) - height(right subtree)| <= 1
binarytree_t *
vector_to_binarytree(binarytree_t *nvec[], int l, int u);

// perform bulk rebalancing by gathering nodes into a vector and
// rebuilding the tree from scratch using the same nodes.
binarytree_t *
binarytree_rebalance(binarytree_t *tree, int count);

// use binarytree_node_cmp to find a matching node in a binary search tree.
// NULL is returned
// if no match is found.
binarytree_t *
binarytree_find(binarytree_t *tree, val_cmp fn, void *val);

// compute a string representing the binary tree printed vertically and
// return result in the treestr parameter.
// caller should provide the appropriate length for treestr.
void
binarytree_tostring(binarytree_t *root,
					val_tostr valtostr_fun, char valstr[], char treestr[]);

void
binarytree_tostring_indent(binarytree_t *tree, val_tostr tostr,
						 char valstr[],	char* indents, char treestr[]);

// compute the height of the binary tree.
// the height of an empty tree is 0.
// the height of an non-empty tree is  1+ the larger of the height of the left subtree
// and the right subtree.
int
binarytree_height(binarytree_t *tree);

// an empty binary tree is balanced.
// a non-empty binary tree is balanced iff the difference in height between
// the left and right subtrees is less or equal to 1.
bool
binarytree_is_balanced(binarytree_t *tree);

// an empty binary tree is in order
// an non-empty binary tree is in order iff
// its left subtree is in order and all of its elements are < the root element
// its right subtree is in order and all of its elements are > the root element
bool
binarytree_is_inorder(binarytree_t *tree, val_cmp compare);


binarytree_t *
binarytree_insert(binarytree_t *tree, val_cmp compare, void *val, mem_alloc m_alloc);

/*
 * if the tree is not NULL, remove the leftmost left node from the tree, make it
 * the new root of the tree, and set its left subtree to the old tree.
 * if the tree is NULL, do nothing.
 */
void
binarytree_leftmostleaf_to_root(binarytree_t **tree);

/*
 * if the tree != NULL, remove and return the leftmost leaf node from the tree,
 * otherwise return NULL.
 */
binarytree_t*
binarytree_remove_leftmostleaf(binarytree_t **tree);

#endif
