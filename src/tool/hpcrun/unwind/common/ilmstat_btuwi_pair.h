/*
 * ilmstat_btuwi_pair.h
 *
 *      Author: dxnguyen
 */

#ifndef __ILMSTAT_BTUWI_PAIR_H__
#define __ILMSTAT_BTUWI_PAIR_H__

//******************************************************************************
// local include files
//******************************************************************************

#include "ildmod_stat.h"
#include "binarytree_uwi.h"


//******************************************************************************
// type
//******************************************************************************

typedef generic_pair_t ilmstat_btuwi_pair_t;

//******************************************************************************
// Constructors
//******************************************************************************

inline ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_new(ildmod_stat_t *key,  bitree_uwi_t *tree,
	mem_alloc m_alloc)
{
  return generic_pair_t_new(key, tree, m_alloc);
}

inline ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, bitree_uwi_t *tree,	mem_alloc m_alloc)
{
  return ilmstat_btuwi_pair_new(
	  ildmod_stat_build(start, end, ldmod, treestat, m_alloc), tree, m_alloc);
}

//******************************************************************************
// Gettors
//******************************************************************************

inline ildmod_stat_t*
ilmstat_btuwi_pair_ilmstat(ilmstat_btuwi_pair_t* itp)
{
  return (ildmod_stat_t*)itp->first;
}

inline bitree_uwi_t*
ilmstat_btuwi_pair_btuwi(ilmstat_btuwi_pair_t* itp)
{
  return (bitree_uwi_t*)itp->second;
}

inline interval_t*
ilmstat_btuwi_pair_interval(ilmstat_btuwi_pair_t* itp)
{
  return ildmod_stat_interval((ildmod_stat_t*)itp->first);
}

inline load_module_t*
ilmstat_btuwi_pair_loadmod(ilmstat_btuwi_pair_t* itp)
{
  return ildmod_stat_loadmod((ildmod_stat_t*)itp->first);
}

inline tree_stat_t
ilmstat_btuwi_pair_stat(ilmstat_btuwi_pair_t* itp)
{
  return ilmstat_btuwi_pair_ilmstat(itp)->stat;
}

uw_recipe_t*
ilmstat_btuwi_pair_recipe(ilmstat_btuwi_pair_t* itp, uintptr_t addr);


//******************************************************************************
// Settors
//******************************************************************************
inline void
ilmstat_btuwi_pair_set_btuwi(ilmstat_btuwi_pair_t* itp, bitree_uwi_t* tree)
{
  itp->second = tree;
}

inline void
ilmstat_btuwi_pair_set_status(ilmstat_btuwi_pair_t* itp, tree_stat_t stat)
{
  ((ildmod_stat_t*)itp->first)->stat = stat;
}
//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: lhs, rhs are ilmstat_btuwi_pair_t*
 */
inline int
ilmstat_btuwi_pair_cmp(void *lhs, void *rhs)
{
  return ildmod_stat_cmp(
	  ((ilmstat_btuwi_pair_t*)lhs)->first,
	  ((ilmstat_btuwi_pair_t*)rhs)->first);
}

/*
 * pre-condition: itp is a ilmstat_btuwi_pair_t*, address is a uintptr_t
 * return interval_ldmod_pair_inrange(itp->first, address)
 */
inline int
ilmstat_btuwi_pair_inrange(void *itp, void *address)
{
  return ildmod_stat_inrange(((ilmstat_btuwi_pair_t*)itp)->first, address);
}

/*
 * return the binary subtree that contains the given address at the root
 */
inline bitree_uwi_t*
ilmstat_btuwi_pair_find(ilmstat_btuwi_pair_t itp, uintptr_t address)
{
  return bitree_uwi_inrange((bitree_uwi_t*)itp.second, address);
}

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: itp is of type ildmod_stat_t*
 */
void
ilmstat_btuwi_pair_tostr(void* itp, char str[]);

/*
 * Compute the string representation of ilmstat_btuwi_pair_t with appropriate
 * indentation of the second component which is a binary tree.
 * Return the result in the parameter str.
 */
void
ilmstat_btuwi_pair_tostr_indent(void* itp, char* indents, char str[]);


void
ilmstat_btuwi_pair_print(ilmstat_btuwi_pair_t* itp);

int
max_ilmstat_btuwi_pair_len();


#endif /* __ILMSTAT_BTUWI_PAIR_H__ */
