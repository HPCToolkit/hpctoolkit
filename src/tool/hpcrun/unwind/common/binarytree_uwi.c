//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> // ptrdiff_t


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
  mem_alloc alloc;
} GF[NUM_UNWINDERS];

static __thread  bitree_uwi_t *_lf_uwi_tree[NUM_UNWINDERS]; // thread local free unwind interval tree

/*
 * initialize the MCS lock for the hidden global free unwind interval tree.
 */
void
bitree_uwi_init(mem_alloc m_alloc)
{
  int i;
  for (i = 0; i < NUM_UNWINDERS; ++i) {
    mcs_init(&GF[i].lock);
    GF[i].tree = NULL;
    GF[i].alloc = m_alloc;
  }
}

// constructors
bitree_uwi_t*
bitree_uwi_malloc(unwinder_t uw,
		  size_t recipe_size)
{
  if (!_lf_uwi_tree[uw]) {
    mcs_node_t me;
    if (mcs_trylock(&GF[uw].lock, &me)) {
      // the global free list is locked, so use it
      _lf_uwi_tree[uw] = GF[uw].tree;
      if (_lf_uwi_tree[uw])
	GF[uw].tree = bitree_uwi_leftsubtree(_lf_uwi_tree[uw]);
      mcs_unlock(&GF[uw].lock, &me);
      if (_lf_uwi_tree[uw])
	bitree_uwi_set_leftsubtree(_lf_uwi_tree[uw], NULL);
    }
    if (!_lf_uwi_tree[uw])
      _lf_uwi_tree[uw] =
	(bitree_uwi_t *)binarytree_listalloc(sizeof(uwi_t) + recipe_size, 
					     NUM_NODES, GF[uw].alloc);
  }

  bitree_uwi_t *top = _lf_uwi_tree[uw];
  if (top) {
    _lf_uwi_tree[uw] = bitree_uwi_rightsubtree(top);
    bitree_uwi_set_rightsubtree(top, NULL);
  }
  return top;
}

/*
 * link only non null tree to GF.tree
 */
void bitree_uwi_free(unwinder_t uw, bitree_uwi_t *tree)
{
  if(!tree) return;
  tree = bitree_uwi_flatten(tree);
  // link to the global free unwind interval tree:
  mcs_node_t me;
  mcs_lock(&GF[uw].lock, &me);
  bitree_uwi_set_leftsubtree(tree, GF[uw].tree);
  GF[uw].tree = tree;
  mcs_unlock(&GF[uw].lock, &me);
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
  if (tree == NULL) return NULL;
  uwi_t* uwi = bitree_uwi_rootval(tree);
  if (uwi == NULL) return NULL;
  return &uwi->interval;
}

// return the recipe_t value of the tree root
// pre-condition: tree != NULL
uw_recipe_t*
bitree_uwi_recipe(bitree_uwi_t *tree)
{
  if (tree == NULL) return NULL;
  uwi_t* uwi = bitree_uwi_rootval(tree);
  if (uwi == NULL) return NULL;
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
uwi_t_any_tostr(void* uwip, char str[], unwinder_t uw)
{
  uwi_t *uwi = uwip;
  char intervalstr[MAX_INTERVAL_STR];
  interval_t_tostr(&uwi->interval, intervalstr);
  char recipestr[MAX_RECIPE_STR];
  uw_recipe_tostr(uwi->recipe, recipestr, uw);
  sprintf(str, "(%s %s)", intervalstr, recipestr);
}

static void
uwi_t_dwarf_tostr(void* uwip, char str[])
{
  uwi_t_any_tostr(uwip, str, DWARF_UNWINDER);
}

static void
uwi_t_native_tostr(void* uwip, char str[])
{
  uwi_t_any_tostr(uwip, str, NATIVE_UNWINDER);
}

static void
(*uwi_t_tostr[NUM_UNWINDERS])(void *uwip, char str[]) =
{
  [DWARF_UNWINDER] = uwi_t_dwarf_tostr,
  [NATIVE_UNWINDER] = uwi_t_native_tostr
};

void
bitree_uwi_tostring_indent(bitree_uwi_t *tree, char *indents,
			   char treestr[], unwinder_t uw)
{
  char uwibuff[MAX_UWI_STR];
  binarytree_tostring_indent((binarytree_t*)tree,
	  uwi_t_tostr[uw], uwibuff, indents, treestr);
}
