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
#define UWI_START_ADDR(btuwi) (bitree_uwi_interval(btuwi))->start
#define UWI_END_ADDR(btuwi) (bitree_uwi_interval(btuwi))->end
#define MAX_RECIPE_STR 256

//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct recipe_s uw_recipe_t;

typedef struct uwi_s {
  interval_t interval;
  char recipe[];
} uwi_t;

typedef struct bitree_uwi_s bitree_uwi_t;

typedef struct btuwi_status_s {
  char *first_undecoded_ins;
  bitree_uwi_t *first;
  int count;
  int error;  // 0 if no error; value = error code or count depending upon unwinder
} btuwi_status_t;

typedef enum unwinder_e {
  DWARF_UNWINDER,
  NATIVE_UNWINDER,
  NUM_UNWINDERS,
} unwinder_t;

/*
 * initialize the MCS lock for the hidden global free unwind interval tree.
 */
void
bitree_uwi_init(mem_alloc m_alloc);

/*
 * Returns a bitree_uwi_t node whose left and right subtree nodes are NULL.
 * The root value of the returned node is a non-null uwi_t*, which is a pair
 * of the form (interval_t* interval, uw_recipe_t* recipe).
 * interval is non-null and *interval = [0, 0).
 * recipe is non-null and points to a concrete instance of an unwind recipe.
 */
bitree_uwi_t*
bitree_uwi_malloc(unwinder_t uw, size_t recipe_size);

/*
 * If tree != NULL return tree to global free tree,
 * otherwise do nothing.
 */
void bitree_uwi_free(unwinder_t uw, bitree_uwi_t *tree);


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

// given a tree that is a list, with all left children empty,
// restructure to make a balanced tree
bitree_uwi_t *
bitree_uwi_rebalance(bitree_uwi_t * tree, int count);

// restructure a binary tree so that all its left children are null
bitree_uwi_t*
bitree_uwi_flatten(bitree_uwi_t * tree);

// use uwi_t_cmp to find a matching node in a binary search tree of uwi_t
// empty tree is returned if no match is found.
bitree_uwi_t*
bitree_uwi_find(bitree_uwi_t *tree, uwi_t *val);

// use uwi_t_inrange to find a node in a binary search tree of uwi_t that
// contains the given address
// empty tree is returned if no such node is found.
bitree_uwi_t*
bitree_uwi_inrange(bitree_uwi_t *tree, uintptr_t address);

/*
 * Concrete implementation of the abstract val_tostr function of the
 * generic_val class.
 * pre-condition: uwr is of type uw_recipe_t*
 */
void
uw_recipe_tostr(void* uwr, char str[]);

void
uw_recipe_print(void* uwr);

// compute a string representing the binary tree printed vertically and
// return result in the treestr parameter.
// caller should provide the appropriate length for treestr.
void
bitree_uwi_tostring(bitree_uwi_t *tree, char treestr[]);

void
bitree_uwi_tostring_indent(bitree_uwi_t *tree, char *indents, char treestr[]);

void
bitree_uwi_print(bitree_uwi_t *tree);

#endif /* __BINARYTREE_UWI_H__ */
