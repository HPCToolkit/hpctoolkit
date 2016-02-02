/*
 * ildmod_btuwi_pair.h
 *
 * ildmod_btuwi_pair_t is a generic pair whose first is a interval_ldmod_pair_t
 * and whose second is a bitree_uwi_t, a binary tree of uwi_t objects as
 * specified in binarytree_uwi.h.
 *
 *      Author: dxnguyen
 */

#ifndef __ILDMODL_BTUWI_PAIR_H__
#define __ILDMODL_BTUWI_PAIR_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdint.h>

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mem_manager.h>
#include <lib/prof-lean/generic_pair.h>
#include "interval_ldmod_pair.h"
#include "binarytree_uwi.h"


//******************************************************************************
// type
//******************************************************************************

typedef generic_pair_t ildmod_btuwi_pair_t;

//******************************************************************************
// Constructors
//******************************************************************************

ildmod_btuwi_pair_t*
ildmod_btuwi_pair_t_new(interval_ldmod_pair_t *key,  bitree_uwi_t *tree,
	mem_alloc m_alloc);

ildmod_btuwi_pair_t*
ildmod_btuwi_pair_t_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	bitree_uwi_t *tree,	mem_alloc m_alloc);


//******************************************************************************
// Gettors
//******************************************************************************

inline interval_ldmod_pair_t*
ildmod_btuwi_pair_ildmod(ildmod_btuwi_pair_t* itp)
{
  return (interval_ldmod_pair_t*) itp->first;
}

inline bitree_uwi_t*
ildmod_btuwi_pair_btuwi(ildmod_btuwi_pair_t* itp)
{
  return (bitree_uwi_t*) itp->second;
}

interval_t*
ildmod_btuwi_pair_interval(ildmod_btuwi_pair_t* itp);

load_module_t*
ildmod_btuwi_pair_loadmod(ildmod_btuwi_pair_t* itp);

uw_recipe_t*
ildmod_btuwi_pair_recipe(ildmod_btuwi_pair_t* itp, uintptr_t addr);


//******************************************************************************
// Settor
//******************************************************************************
inline void
ildmod_btuwi_pair_set_btuwi(ildmod_btuwi_pair_t* itp, bitree_uwi_t* tree)
{
  itp->second = tree;
}


//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: itp1, itp2 are ildmod_btuwi_pair_t*
 * return ptrinterval_cmp(itp1->first, itp2->first)
 */
int
ildmod_btuwi_pair_cmp(void *itp1, void *itp2);

/*
 * pre-condition: itp is a ildmod_btuwi_pair_t*, address is a uintptr_t
 * return interval_ldmod_pair_inrange(itp->first, address)
 */
int
ildmod_btuwi_pair_inrange(void *itp, void *address);

/*
 * return the binary subtree that contains the given address
 */
bitree_uwi_t*
ildmod_btuwi_pair_find(ildmod_btuwi_pair_t itp, uintptr_t address);

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: itp is of type ildmod_btuwi_pair_t*
 */
void
ildmod_btuwi_pair_tostr(void* itp, char str[]);

/*
 * Compute the string representation of ildmod_btuwi_pair_t with appropriate
 * indentation of the second component which is a binary tree.
 * Return the result in the parameter str.
 */
void
ildmod_btuwi_pair_tostr_indent(void* itp, char* indents, char str[]);


void
ildmod_btuwi_pair_print(ildmod_btuwi_pair_t* itp);

int
max_ildmod_btuwi_pair_len();

#endif /* __ILDMODL_BTUWI_PAIR_H__ */
