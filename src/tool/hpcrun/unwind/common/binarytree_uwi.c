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

#include <lib/prof-lean/mcs-lock.h>
#include <lib/prof-lean/binarytree.h>
#include "binarytree_uwi.h"

#define NUM_NODES 10
#define BTUWI_DEBUG 0

static bitree_uwi_t *GF_uwi_tree = NULL; // global free unwind interval tree
static mcs_lock_t GFT_lock;  // lock for GF_uwi_tree
static __thread  bitree_uwi_t *_lf_uwi_tree = NULL;  // thread local free unwind interval tree

/*
 * initialize the MCS lock for the hidden global free unwind interval tree.
 */
void
bitree_uwi_init()
{
#if BTUWI_DEBUG
  printf("DXN_DBG: bitree_uwi_init mcs_init(&GFT_lock) \n");
#endif
  mcs_init(&GFT_lock);
}

// constructors
static bitree_uwi_t*
bitree_uwi_new_node(mem_alloc m_alloc, size_t recipe_size)
{
  bitree_uwi_t *btuwi = (bitree_uwi_t *)binarytree_new(sizeof(uwi_t) + recipe_size, m_alloc);
  uwi_t* uwi = bitree_uwi_rootval(btuwi);
  uwi->interval.start = uwi->interval.end = 0;
  return btuwi;
}

static void
bitree_uwi_add_nodes_to_lft(
	mem_alloc m_alloc,
	size_t recipe_size)
{
  bitree_uwi_t *btuwi = bitree_uwi_new_node(m_alloc, recipe_size);
  bitree_uwi_t * current = btuwi;
  for (int i = 1; i < NUM_NODES; i++) {
	bitree_uwi_t *new_node =  bitree_uwi_new_node(m_alloc, recipe_size);
	bitree_uwi_set_leftsubtree(current, new_node);
	current = new_node;
  }
  bitree_uwi_set_rightsubtree(btuwi, _lf_uwi_tree);
  _lf_uwi_tree = btuwi;
}

static bitree_uwi_t *
bitree_uwi_alloc_from_lft()
{
  return bitree_uwi_remove_leftmostleaf(&_lf_uwi_tree);
}

static void
bitree_uwi_populate_lft(
	mem_alloc m_alloc,
	size_t recipe_size)
{
  mcs_node_t me;
  bool acquired = mcs_trylock(&GFT_lock, &me);
  if (acquired) {
	// the global free list is locked, so use it
	if (GF_uwi_tree) {
	  bitree_uwi_t *btuwi = GF_uwi_tree;
	  GF_uwi_tree = bitree_uwi_rightsubtree(GF_uwi_tree);
	  mcs_unlock(&GFT_lock, &me);
	  bitree_uwi_set_rightsubtree(btuwi, _lf_uwi_tree);
	  _lf_uwi_tree = btuwi;
	}
	else {
	  mcs_unlock(&GFT_lock, &me);
	}
  }
  if (!_lf_uwi_tree) {
	bitree_uwi_add_nodes_to_lft(m_alloc, recipe_size);
  }
}

bitree_uwi_t*
bitree_uwi_malloc(
	mem_alloc m_alloc,
	size_t recipe_size)
{
  if (!_lf_uwi_tree) {
	bitree_uwi_populate_lft(m_alloc, recipe_size);
  }

#if BTUWI_DEBUG
  assert(_lf_uwi_tree != NULL);
#endif

  return bitree_uwi_alloc_from_lft();
}

// destructor
void
bitree_uwi_del(bitree_uwi_t **tree, mem_free m_free)
{
  binarytree_del((binarytree_t**) tree, m_free);
}

/*
 * link only non null tree to GF_uwi_tree
 */
void bitree_uwi_free(bitree_uwi_t *tree)
{
  if(!tree) return;
  bitree_uwi_leftmostleaf_to_root(&tree);
  // link to the global free unwind interval tree:
  mcs_node_t me;
  mcs_lock(&GFT_lock, &me);
  bitree_uwi_set_rightsubtree(tree, GF_uwi_tree);
  GF_uwi_tree = tree;
  bitree_uwi_set_rightsubtree(tree, NULL);
  mcs_unlock(&GFT_lock, &me);
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

void
bitree_uwi_set_leftsubtree(
	bitree_uwi_t *tree,
	bitree_uwi_t* subtree)
{
  binarytree_set_leftsubtree((binarytree_t*) tree, (binarytree_t*)subtree);
}

void
bitree_uwi_set_rightsubtree(
	bitree_uwi_t *tree,
	bitree_uwi_t* subtree)
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
  return &uwi->interval;
}

// return the recipe_t value of the tree root
// pre-condition: tree != NULL
uw_recipe_t*
bitree_uwi_recipe(bitree_uwi_t *tree)
{
  assert(tree != NULL);
  uwi_t* uwi = bitree_uwi_rootval(tree);
  assert(uwi != NULL);
  return (uw_recipe_t *)uwi->recipe;
}

// perform bulk rebalancing by gathering nodes into a vector and
// rebuilding the tree from scratch using the same nodes.
bitree_uwi_t*
bitree_uwi_rebalance(bitree_uwi_t * tree, int count)
{
  binarytree_t *balanced = binarytree_list_to_tree((binarytree_t**)&tree, count);
  return (bitree_uwi_t*)balanced;
}

static int
uwi_t_cmp(void* lhs, void* rhs)
{
  uwi_t* uwi1 = (uwi_t*)lhs;
  uwi_t* uwi2 = (uwi_t*)rhs;
  return interval_t_cmp(&uwi1->interval, &uwi2->interval);
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
static int
uwi_t_inrange(void* lhs, void* address)
{
  uwi_t* uwi = (uwi_t*)lhs;
  return interval_t_inrange(&uwi->interval, address);
}

bitree_uwi_t*
bitree_uwi_inrange(bitree_uwi_t *tree, uintptr_t address)
{
  binarytree_t * found =
	  binarytree_find((binarytree_t*)tree, uwi_t_inrange, (void*)address);
  return (bitree_uwi_t*)found;
}


//******************************************************************************
// String output
//******************************************************************************

#define MAX_UWI_STR MAX_INTERVAL_STR+MAX_RECIPE_STR+4
static void
uwi_t_tostr(void* uwip, char str[])
{
  uwi_t *uwi = uwip;
  char intervalstr[MAX_INTERVAL_STR];
  interval_t_tostr(&uwi->interval, intervalstr);
  char recipestr[MAX_RECIPE_STR];
  uw_recipe_tostr(uwi->recipe, recipestr);
  sprintf(str, "(%s %s)", intervalstr, recipestr);
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


void
bitree_uwi_leftmostleaf_to_root(bitree_uwi_t **tree)
{
  binarytree_leftmostleaf_to_root((binarytree_t**)tree);
}

bitree_uwi_t*
bitree_uwi_remove_leftmostleaf(bitree_uwi_t **tree)
{
  return (bitree_uwi_t*) binarytree_remove_leftmostleaf((binarytree_t**)tree);
}
