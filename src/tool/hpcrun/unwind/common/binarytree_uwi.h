/*
 * binarytree_uwi.h
 *
 * binary search tree of unwind intervals uwi_t as specified in uwi.h.
 *
 * Author: dxnguyen
 */

#ifndef __BINARYTREE_UWI_H__
#define __BINARYTREE_UWI_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdint.h>
#include <stdbool.h>


//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mem_manager.h>
#include "interval_t.h"
#include "uw_recipe.h"


/******************************************************************************
 * macros to convert the old unwind interval data structure
 *

struct unwind_interval_t {
  struct splay_interval_s common;
  ra_loc ra_status;
  int sp_ra_pos;
  int sp_bp_pos;
  bp_loc bp_status;
  int bp_ra_pos;
  int bp_bp_pos;
  struct unwind_interval_t *prev_canonical;
  int restored_canonical;
  bool has_tail_calls;
};
typedef struct unwind_interval_t unwind_interval;

 * to the new one, which is bitree_uwi_t, a binary tree of uwi_t.
 *
 ******************************************************************************/

#define UWI_NEXT(btuwi) (bitree_uwi_rightsubtree(btuwi))
#define UWI_PREV(btuwi) (bitree_uwi_leftsubtree(btuwi))
#define UWI_START_ADDR(btuwi) (bitree_uwi_interval(btuwi))->start
#define UWI_END_ADDR(btuwi) (bitree_uwi_interval(btuwi))->end


//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct uwi_s {
  interval_t *interval;
  uw_recipe_t *recipe;
} uwi_t;

typedef struct bitree_uwi_s bitree_uwi_t;

// to replace the old interval_status_t:
typedef struct btuwi_status_s {
  char *first_undecoded_ins;
  bitree_uwi_t *first;
  int errcode;
} btuwi_status_t;

/*
 * initialize the MCS lock for the hidden global free unwind interval tree.
 */
void
bitree_uwi_init();

// constructors
bitree_uwi_t*
bitree_uwi_new(uwi_t *val, bitree_uwi_t *left, bitree_uwi_t *right,
	mem_alloc m_alloc);

/*
 * Returns a bitree_uwi_t node whose left and right subtree nodes are NULL.
 * The root value of the returned node is a non-null uwi_t*, which is a pair
 * of the form (interval_t* interval, uw_recipe_t* recipe).
 * interval is non-null and *interval = [0, 0).
 * recipe is non-null and points to a concrete instance of an unwind recipe.
 */
bitree_uwi_t*
bitree_uwi_malloc(mem_alloc m_alloc, size_t recipe_size);

// destructor
void bitree_uwi_del(bitree_uwi_t **tree, mem_free m_free);

/*
 * If tree != NULL return tree to global free tree,
 * otherwise do nothing.
 */
void bitree_uwi_free(bitree_uwi_t *tree);


// return the value at the root
// pre-condition: tree != NULL
uwi_t*
bitree_uwi_rootval(bitree_uwi_t *tree);

// pre-condition: tree != NULL
bitree_uwi_t*
bitree_uwi_leftsubtree(bitree_uwi_t *tree);

// pre-condition: tree != NULL
bitree_uwi_t*
bitree_uwi_rightsubtree(bitree_uwi_t *tree);

void
bitree_uwi_set_rootval(
	bitree_uwi_t *tree,
	uwi_t* rootval);

void
bitree_uwi_set_leftsubtree(
	bitree_uwi_t *tree,
	bitree_uwi_t* subtree);

void
bitree_uwi_set_rightsubtree(
	bitree_uwi_t *tree,
	bitree_uwi_t* subtree);

// return the interval_t part of the interval_t key of the tree root
// pre-condition: tree != NULL
interval_t*
bitree_uwi_interval(bitree_uwi_t *tree);

// return the recipe_t value of the tree root
// pre-condition: tree != NULL
uw_recipe_t*
bitree_uwi_recipe(bitree_uwi_t *tree);

// count the number of nodes in the binary tree.
int
bitree_uwi_count(bitree_uwi_t * tree);

// perform bulk rebalancing by gathering nodes into a vector and
// rebuilding the tree from scratch using the same nodes.
bitree_uwi_t *
bitree_uwi_rebalance(bitree_uwi_t * tree);

// use uwi_t_cmp to find a matching node in a binary search tree of uwi_t
// empty tree is returned if no match is found.
bitree_uwi_t*
bitree_uwi_find(bitree_uwi_t *tree, uwi_t *val);

// use uwi_t_inrange to find a node in a binary search tree of uwi_t that
// contains the given address
// empty tree is returned if no such node is found.
bitree_uwi_t*
bitree_uwi_inrange(bitree_uwi_t *tree, uintptr_t address);

// compute a string representing the binary tree printed vertically and
// return result in the treestr parameter.
// caller should provide the appropriate length for treestr.
void
bitree_uwi_tostring(bitree_uwi_t *tree, char treestr[]);

void
bitree_uwi_tostring_indent(bitree_uwi_t *tree, char *indents, char treestr[]);

void
bitree_uwi_print(bitree_uwi_t *tree);

// compute the height of the binary tree.
// the height of an empty tree is 0.
// the height of an non-empty tree is  1+ the larger of the height of the left subtree
// and the right subtree.
int
bitree_uwi_height(bitree_uwi_t *tree);

// an empty binary tree is balanced.
// a non-empty binary tree is balanced iff the difference in height between
// the left and right subtrees is less or equal to 1.
bool
bitree_uwi_is_balanced(bitree_uwi_t *tree);

// an empty binary tree is in order
// an non-empty binary tree is in order iff
// its left subtree is in order and all of its elements are < the root element
// its right subtree is in order and all of its elements are > the root element
bool
bitree_uwi_is_inorder(bitree_uwi_t *tree);


bitree_uwi_t *
bitree_uwi_insert(bitree_uwi_t *tree, uwi_t *val, mem_alloc m_alloc);

void
bitree_uwi_leftmostleaf_to_root(bitree_uwi_t **tree);

bitree_uwi_t*
bitree_uwi_remove_leftmostleaf(bitree_uwi_t **tree);

#endif /* __BINARYTREE_UWI_H__ */
