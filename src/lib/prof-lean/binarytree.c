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
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//******************************************************************************
// local include files 
//******************************************************************************

#include "binarytree.h"


//******************************************************************************
// macros 
//******************************************************************************

#define MAX_SUBTREE_STR 32768
#define MAX_LEFT_LEAD_STR 256

//******************************************************************************
// implementation type
//******************************************************************************

#if 0
// DXN: moved to header file
// DXN: opaque type not supported by gcc 4.4.*

typedef struct binarytree_s {
  struct binarytree_s *left;
  struct binarytree_s *right;
  void* val;
} binarytree_t;

#endif


//******************************************************************************
// Abstract functions
//******************************************************************************

// reserved for treaps where a priority is associated with each value in each node.
typedef int (*get_priority)(binarytree_t* treapnode);



//******************************************************************************
// private operations
//******************************************************************************

// perform an in order traversal and collect nodes into a vector
static void
tree_to_vector_helper
(binarytree_t *nvec[],
 binarytree_t *node,
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
subtree_tostr2(binarytree_t *subtree, val_tostr tostr, char valstr[],
			   char* left_lead, char result[])
{
  if (subtree) {
    char Left_subtree_buff[MAX_SUBTREE_STR + 1];
	char Right_subtree_buff[MAX_SUBTREE_STR + 1];
	char Left_lead_buff[MAX_LEFT_LEAD_STR + 1];

    size_t new_left_lead_size = strlen(left_lead) + strlen("|  ") + 1;
    snprintf(Left_lead_buff, new_left_lead_size, "%s%s", left_lead, "|  ");
    binarytree_t * left = {subtree->left};
    subtree_tostr2(left, tostr, valstr, Left_lead_buff, Left_subtree_buff);
    snprintf(Left_lead_buff, new_left_lead_size, "%s%s", left_lead, "   ");
    binarytree_t * right = {subtree->right};
    subtree_tostr2(right, tostr, valstr, Left_lead_buff, Right_subtree_buff);
	tostr(subtree->val, valstr);
    snprintf(result, MAX_TREE_STR, "%s%s%s%s%s%s%s%s%s%s%s%s",
    		"|_ ", valstr, "\n",
			left_lead, "|\n",
			left_lead, Left_subtree_buff, "\n",
			left_lead, "|\n",
			left_lead, Right_subtree_buff);
  }
  else {
    strcpy(result, "|_ {}");
  }
}

//******************************************************************************
// interface operations
//******************************************************************************


binarytree_t *
binarytree_new(void *value, binarytree_t *left, binarytree_t *right,
	mem_alloc m_alloc)
{
  binarytree_t *node = (binarytree_t *)m_alloc(sizeof(binarytree_t));
  node->right = right;
  node->left  = left;
  node->val   = value;
  return node;
}

// destructor
void binarytree_del(binarytree_t **root, mem_free m_free)
{
  if (*root) {
	binarytree_del(&((*root)->left), m_free);
	binarytree_del(&((*root)->right), m_free);
	if ((*root)->val) {
	  m_free((*root)->val);
	}
	m_free(*root);
	*root = NULL;
  }
}

// return the value at the root
// pre-condition: tree != NULL
void*
binarytree_rootval(binarytree_t *tree)
{
  assert(tree != NULL);
  return tree->val;
}

// pre-condition: tree != NULL
binarytree_t*
binarytree_leftsubtree(binarytree_t *tree)
{
  assert(tree != NULL);
  return tree->left;
}

// pre-condition: tree != NULL
binarytree_t*
binarytree_rightsubtree(binarytree_t *tree)
{
  assert(tree != NULL);
  return tree->right;
}

void
binarytree_set_rootval(binarytree_t *tree, void* rootval)
{
  assert(tree != NULL);
  tree->val = rootval;
}

void
binarytree_set_leftsubtree(binarytree_t *tree, binarytree_t* subtree)
{
  assert(tree != NULL);
  tree->left = subtree;
}

void
binarytree_set_rightsubtree(binarytree_t *tree, binarytree_t* subtree)
{
  assert(tree != NULL);
  tree->right = subtree;
}

int
binarytree_count(binarytree_t *tree)
{
  return tree?
	  binarytree_count(tree->left) + binarytree_count(tree->right) + 1: 0;

#if 0
  if (!tree) {
	return 0;
  }
  else {
	return binarytree_count(tree->left) + binarytree_count(tree->right) + 1;
  }
#endif
}

void
binarytree_to_vector(binarytree_t *nvec[], binarytree_t *tree)
{
  int count = 0;
  tree_to_vector_helper(nvec, tree, &count);
}

// post-condition: 0 <= |depth(left subtree) - depth(right subtree)| <= 1
binarytree_t *
vector_to_binarytree(binarytree_t *nvec[], int lo, int hi)
{
  // the midpoint of the subvector is the root of the subtree
  int mid = ((hi - lo) >> 1) + lo;
  binarytree_t *root = nvec[mid];

  // build left subtree, if any, from nodes to the left of the midpoint
  root->left  = (mid != lo) ? vector_to_binarytree(nvec, lo, mid - 1) : NULL;

  // build right subtree, if any, from nodes to the right of the midpoint
  root->right = (mid != hi) ? vector_to_binarytree(nvec, mid + 1, hi) : NULL;

  return root;
}

binarytree_t *
binarytree_rebalance(binarytree_t * root)
{
  if (!root) return root;

  int n = binarytree_count(root);

  // collect tree nodes in order into a vector
  binarytree_t *vec[n];
  binarytree_to_vector(vec, root);

  // construct a balanced binary tree from an ordered vector of nodes
  return vector_to_binarytree(vec, 0, n - 1);
}


binarytree_t *
binarytree_find(binarytree_t * root,	val_cmp matches, void *val)
{
  while (root) {
    int cmp_status = matches(root->val, val);
  
    if (cmp_status == 0) return root; // subtree_root matches val
  
    // determine which subtree to search
    root = (cmp_status > 0) ? root->left : root->right;
  }
  return NULL;
}

void
binarytree_tostring(binarytree_t * tree, val_tostr tostr, char valstr[],
	char result[])
{
  binarytree_tostring_indent(tree, tostr, valstr, "", result);
}

void
binarytree_tostring_indent(binarytree_t * root, val_tostr tostr,
						 char valstr[],	char* indents, char result[])
{
  if (root) {
    char Left_subtree_buff[MAX_SUBTREE_STR + 1];
	char Right_subtree_buff[MAX_SUBTREE_STR + 1];
	char newindents[MAX_INDENTS+5];
	snprintf(newindents, MAX_INDENTS+4, "%s%s", indents, "|  ");
	subtree_tostr2(root->left, tostr, valstr, newindents, result);
    snprintf(Left_subtree_buff, MAX_SUBTREE_STR, "%s", result);
	snprintf(newindents, MAX_INDENTS+4, "%s%s", indents, "   ");
	subtree_tostr2(root->right, tostr, valstr, newindents, result);
	snprintf(Right_subtree_buff, MAX_SUBTREE_STR, "%s", result);
	tostr(root->val, valstr);
    snprintf(result, MAX_TREE_STR, "%s%s%s%s%s%s%s%s%s%s%s",
    		valstr, "\n",
			indents,"|\n",
    		indents, Left_subtree_buff, "\n",
			indents, "|\n",
			indents, Right_subtree_buff);
  }
  else {
	strcpy(result, "{}");
  }
}

int
binarytree_height(binarytree_t *root)
{
  if (root) {
	int left_height = binarytree_height(root->left);
	int right_height = binarytree_height(root->right);
	return (left_height > right_height)? left_height + 1: right_height + 1;
  }
  return 0;
}

bool
binarytree_is_balanced(binarytree_t *root)
{
  if (root) {
	int ldepth = binarytree_height(root->left);
	int rdepth = binarytree_height(root->right);
	if (rdepth - ldepth > 1) return 0;
	if (!binarytree_is_balanced(root->left)) return 0;
	if (!binarytree_is_balanced(root->right)) return 0;
  }
  return 1;
}

bool
binarytree_is_inorder(binarytree_t *root, val_cmp compare)
{
  if (!root) return true; // empty tree is trivially in order
  if (root->left) {
	// value at node is smaller than left child's
	if (compare(root->left->val, root->val) >= 0) return false;
	// left subtree is not in order
	if (!binarytree_is_inorder(root->left, compare)) return false;
  }
  if (root->right) {
	// value at node is larger than right child's
	if (compare(root->right->val, root->val) <= 0) return false;
	// right subtree is not in order
	if (!binarytree_is_inorder(root->right, compare)) return false;
  }
  return true; // tree is in order
}

binarytree_t *
binarytree_insert(binarytree_t *root, val_cmp compare, void *val, mem_alloc m_alloc)
{
  if(!root) {
	return binarytree_new(val, NULL, NULL, m_alloc);  // empty tree case
  }
  else if (compare(root->val, val) > 0) {
    root->left = binarytree_insert(root->left, compare, val, m_alloc);
  }
  else if (compare(root->val, val) < 0) {
	root->right = binarytree_insert(root->right, compare, val, m_alloc);
  }
  return root;
}


#if 0

//******************************************************************************
// Abstract functions
//******************************************************************************

// reserved for treaps where a priority is associated with each value in each node.
typedef int (*get_priority)(binarytree_t* treapnode);



static void
split(binarytree_t** treap, binarytree_node_cmp compare, void* key,
		binarytree_t** left, binarytree_t** right)
  {
	binarytree_t** subtree;
    if (*treap == NULL){
      *left = *right = NULL;
    }
    else if (compare(*treap, key) > 0) {
      subtree = &((*treap)->left);
      split(subtree, compare, key, left, subtree);
      *right = *treap;
    }
    else {
      subtree = &((*treap)->right);
      split(subtree, compare, key, subtree, right);
      *left = *treap;
    }
  }

// heap property: root heap priority >= subtrees' priorities
void
binarytree_treapinsert(binarytree_t **treap, binarytree_node_cmp compare,
		get_priority getprior, binarytree_t* newnode) {
  if (*treap == NULL) {
	*treap = newnode;   // empty tree case
	return;
  }
  if (getprior(*treap) > getprior(newnode)) {
	binarytree_t** subtree = compare(*treap, newnode->val) > 0?
										&((*treap)->left): &((*treap)->right);
	binarytree_treapinsert(subtree, compare, getprior, newnode);
  }
  else {
	// do some tree splitting here
	split(treap, compare, newnode->val, &(newnode->left), &(newnode->right));
	*treap = newnode;
  }
}
#endif

