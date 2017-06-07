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

static struct {
  bitree_uwi_t *tree;		// global free unwind interval tree
  mcs_lock_t lock;		// lock for tree
} GF;

static __thread  bitree_uwi_t *_lf_uwi_tree = NULL;  // thread local free unwind interval tree

/*
 * initialize the MCS lock for the hidden global free unwind interval tree.
 */
void
bitree_uwi_init()
{
  mcs_init(&GF.lock);
}

// constructors
bitree_uwi_t*
bitree_uwi_malloc(
	mem_alloc m_alloc,
	size_t recipe_size)
{
  if (!_lf_uwi_tree) {
    mcs_node_t me;
    if (mcs_trylock(&GF.lock, &me)) {
      // the global free list is locked, so use it
      _lf_uwi_tree = GF.tree;
      if (_lf_uwi_tree)
	GF.tree = bitree_uwi_leftsubtree(_lf_uwi_tree);
      mcs_unlock(&GF.lock, &me);
      if (_lf_uwi_tree)
	bitree_uwi_set_leftsubtree(_lf_uwi_tree, NULL);
    }
    if (!_lf_uwi_tree)
      _lf_uwi_tree =
	(bitree_uwi_t *)binarytree_listalloc(sizeof(uwi_t) + recipe_size, 
					     NUM_NODES, m_alloc);
  }

  bitree_uwi_t *top = _lf_uwi_tree;
  if (top) {
    _lf_uwi_tree = bitree_uwi_rightsubtree(top);
    bitree_uwi_set_rightsubtree(top, NULL);
  }
  return top;
}

/*
 * link only non null tree to GF.tree
 */
void bitree_uwi_free(bitree_uwi_t *tree)
{
  if(!tree) return;
  tree = bitree_uwi_flatten(tree);
  // link to the global free unwind interval tree:
  mcs_node_t me;
  mcs_lock(&GF.lock, &me);
  bitree_uwi_set_leftsubtree(tree, GF.tree);
  GF.tree = tree;
  mcs_unlock(&GF.lock, &me);
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

// change a tree of all right children into a balanced tree
bitree_uwi_t*
bitree_uwi_rebalance(bitree_uwi_t * tree, int count)
{
  binarytree_t *balanced = binarytree_list_to_tree((binarytree_t**)&tree, count);
  return (bitree_uwi_t*)balanced;
}

bitree_uwi_t*
bitree_uwi_flatten(bitree_uwi_t * tree)
{
  binarytree_t *flattened = binarytree_listify((binarytree_t*)tree);
  return (bitree_uwi_t*)flattened;
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
