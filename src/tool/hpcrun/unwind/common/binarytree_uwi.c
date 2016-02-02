/*
 * binarytree_uwi.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> // ptrdiff_t
#include <assert.h>


//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/binarytree.h>
#include "binarytree_uwi.h"


// constructor
bitree_uwi_t*
bitree_uwi_new(uwi_t *val,
	bitree_uwi_t *left, bitree_uwi_t *right, mem_alloc m_alloc)
{
  binarytree_t *bt =
	  binarytree_new(val, (binarytree_t*)left, (binarytree_t*)right, m_alloc);
  return (bitree_uwi_t*)bt;
}

// destructor
void
bitree_uwi_del(bitree_uwi_t **tree, mem_free m_free)
{
  binarytree_del((binarytree_t**) tree, m_free);
}

// return the value at the root
// pre-condition: tree != NULL
uwi_t*
bitree_uwi_rootval(bitree_uwi_t *tree)
{
  return (uwi_t*) binarytree_rootval((binarytree_t*) tree);
}

// pre-condition: tree != NULL
bitree_uwi_t*
bitree_uwi_leftsubtree(bitree_uwi_t *tree)
{
  return (bitree_uwi_t*) binarytree_leftsubtree((binarytree_t*) tree);
}

// pre-condition: tree != NULL
bitree_uwi_t*
bitree_uwi_rightsubtree(bitree_uwi_t *tree)
{
  return (bitree_uwi_t*) binarytree_rightsubtree((binarytree_t*) tree);
}

void*
bitree_uwi_set_rootval(bitree_uwi_t *tree, uwi_t* rootval)
{
  binarytree_set_rootval((binarytree_t*) tree, rootval);
}

void*
bitree_uwi_set_leftsubtree(bitree_uwi_t *tree, bitree_uwi_t* subtree)
{
  binarytree_set_leftsubtree((binarytree_t*) tree, (binarytree_t*)subtree);
}

void*
bitree_uwi_set_rightsubtree(bitree_uwi_t *tree, bitree_uwi_t* subtree)
{
  binarytree_set_rightsubtree((binarytree_t*) tree, (binarytree_t*)subtree);
}

// return the interval_t part of the interval_t key of the tree root
// pre-condition: tree != NULL
interval_t*
bitree_uwi_interval(bitree_uwi_t *tree)
{
  assert(tree != NULL);
  uwi_t* uwi = bitree_uwi_rootval(tree);
  assert(uwi != NULL);
  return uwi_t_interval(uwi);
}

// return the recipe_t value of the tree root
// pre-condition: tree != NULL
uw_recipe_t*
bitree_uwi_recipe(bitree_uwi_t *tree)
{
  assert(tree != NULL);
  uwi_t* uwi = bitree_uwi_rootval(tree);
  assert(uwi != NULL);
  return uwi_t_recipe(uwi);
}

// count the number of nodes in the binary tree.
int
bitree_uwi_count(bitree_uwi_t *tree)
{
  return binarytree_count((binarytree_t *)tree);
}

// perform bulk rebalancing by gathering nodes into a vector and
// rebuilding the tree from scratch using the same nodes.
bitree_uwi_t*
bitree_uwi_rebalance(bitree_uwi_t * tree)
{
  binarytree_t *balanced = binarytree_rebalance((binarytree_t*)tree);
  return (bitree_uwi_t*)balanced;
}

// use uwi_t_cmp to find a matching node in a binary search tree of uwi_t
// empty tree is returned if no match is found.
bitree_uwi_t*
bitree_uwi_find(bitree_uwi_t *tree, uwi_t *val)
{
  binarytree_t *found =
	  binarytree_find((binarytree_t*)tree, uwi_t_cmp, val);
  return (bitree_uwi_t*)found;
}

// use uwi_t_inrange to find a node in a binary search tree of uwi_t that
// contains the given address
// empty tree is returned if no such node is found.
bitree_uwi_t*
bitree_uwi_inrange(bitree_uwi_t *tree, uintptr_t address)
{
  binarytree_t * found =
	  binarytree_find((binarytree_t*)tree, uwi_t_inrange, (void*)address);
  return (bitree_uwi_t*)found;
}

// compute a string representing the binary tree printed vertically and
// return result in the treestr parameter.
// caller should provide the appropriate length for treestr.
void
bitree_uwi_tostring(bitree_uwi_t *tree, char treestr[])
{
  char uwibuff[MAX_UWI_STR];
  binarytree_tostring((binarytree_t*)tree,
	  uwi_t_tostr, uwibuff, treestr);
}

void
bitree_uwi_print(bitree_uwi_t *tree) {
  char treestr[MAX_TREE_STR];
  bitree_uwi_tostring(tree, treestr);
  printf("%s\n\n", treestr);
}

void
bitree_uwi_tostring_indent(bitree_uwi_t *tree, char *indents,
	char treestr[])
{
  char uwibuff[MAX_UWI_STR];
  binarytree_tostring_indent((binarytree_t*)tree,
	  uwi_t_tostr, uwibuff, indents, treestr);
}

// compute the height of the binary tree.
// the height of an empty tree is 0.
// the height of an non-empty tree is  1+ the larger of the height of the left subtree
// and the right subtree.
int
bitree_uwi_height(bitree_uwi_t *tree)
{
  return binarytree_height((binarytree_t*)tree);
}

// an empty binary tree is balanced.
// a non-empty binary tree is balanced iff the difference in height between
// the left and right subtrees is less or equal to 1.
bool
bitree_uwi_is_balanced(bitree_uwi_t *tree)
{
  return binarytree_is_balanced((binarytree_t*)tree);
}

// an empty binary tree is in order
// an non-empty binary tree is in order iff
// its left subtree is in order and all of its elements are < the root element
// its right subtree is in order and all of its elements are > the root element
bool
bitree_uwi_is_inorder(bitree_uwi_t *tree)
{
  return binarytree_is_inorder((binarytree_t*)tree, uwi_t_cmp);
}


bitree_uwi_t*
bitree_uwi_insert(bitree_uwi_t *tree, uwi_t *val, mem_alloc m_alloc)
{
  return (bitree_uwi_t*)binarytree_insert((binarytree_t*)tree,
	  uwi_t_cmp, val, m_alloc);
}

bitree_uwi_t*
bitree_uwi_finalize(bitree_uwi_t *tree)
{
  if (tree == NULL) return tree;
  bitree_uwi_set_leftsubtree(tree, NULL);
  bitree_uwi_finalize(bitree_uwi_rightsubtree(tree));
  return tree;
}
