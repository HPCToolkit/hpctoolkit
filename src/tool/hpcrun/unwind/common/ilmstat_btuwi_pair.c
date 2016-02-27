/*
 * ilmstat_btuwi_pair.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/binarytree.h>
#include "ilmstat_btuwi_pair.h"

//******************************************************************************
// Gettors (externs are inlined)
//******************************************************************************

extern ildmod_stat_t* ilmstat_btuwi_pair_ilmstat(ilmstat_btuwi_pair_t* itp);
extern bitree_uwi_t* ilmstat_btuwi_pair_btuwi(ilmstat_btuwi_pair_t* itp);
extern interval_t* ilmstat_btuwi_pair_interval(ilmstat_btuwi_pair_t* itp);
extern load_module_t* ilmstat_btuwi_pair_loadmod(ilmstat_btuwi_pair_t* itp);
extern tree_stat_t ilmstat_btuwi_pair_stat(ilmstat_btuwi_pair_t* itp);

uw_recipe_t*
ilmstat_btuwi_pair_recipe(ilmstat_btuwi_pair_t* itp, uintptr_t addr)
{
  if (itp) {
	bitree_uwi_t *found =
		bitree_uwi_inrange(ilmstat_btuwi_pair_btuwi(itp), addr);
	return found? bitree_uwi_recipe(found): NULL;
  }
  return NULL;
}

//******************************************************************************
// Settors
//******************************************************************************
extern void ilmstat_btuwi_pair_set_btuwi(ilmstat_btuwi_pair_t* itp, bitree_uwi_t* tree);
extern void ilmstat_btuwi_pair_set_status(ilmstat_btuwi_pair_t* itp, tree_stat_t stat);

//******************************************************************************
// Constructors
//******************************************************************************

extern ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_new(ildmod_stat_t *key,  bitree_uwi_t *tree,
	mem_alloc m_alloc);

extern ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, bitree_uwi_t *tree,	mem_alloc m_alloc);


//******************************************************************************
// Comparators
//******************************************************************************

extern int ilmstat_btuwi_pair_cmp(void *lhs, void *rhs);
extern int ilmstat_btuwi_pair_inrange(void *itp, void *address);
extern bitree_uwi_t* ilmstat_btuwi_pair_find(ilmstat_btuwi_pair_t itp, uintptr_t address);

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: itp is of type ildmod_stat_t*
 */
void
ilmstat_btuwi_pair_tostr(void* itp, char str[])
{
  ilmstat_btuwi_pair_tostr_indent(itp, ildmod_stat_maxspaces(), str);
}

/*
 * Compute the string representation of ilmstat_btuwi_pair_t with appropriate
 * indentation of the second component which is a binary tree.
 * Return the result in the parameter str.
 */
void
ilmstat_btuwi_pair_tostr_indent(void* itp, char* indents, char str[])
{
  ilmstat_btuwi_pair_t* it_pair = (ilmstat_btuwi_pair_t*)itp;
  ildmod_stat_t *ilms = (ildmod_stat_t*)it_pair->first;
  bitree_uwi_t *tree = (bitree_uwi_t*)it_pair->second;
  char firststr[MAX_ILDMODSTAT_STR];
  char secondstr[MAX_TREE_STR];
  ildmod_stat_tostr(ilms, firststr);
  bitree_uwi_tostring_indent(tree, indents, secondstr);
  snprintf(str, strlen(firststr) + strlen(secondstr) + 6, "%s%s%s%s%s",
	  "(", firststr, ",  ", secondstr, ")");
}

void
ilmstat_btuwi_pair_print(ilmstat_btuwi_pair_t* itp)
{
  char str[MAX_ILDMODSTAT_STR + MAX_TREE_STR + 4];
  ilmstat_btuwi_pair_tostr(itp, str);
  printf("%s", str);
}

int
max_ilmstat_btuwi_pair_len()
{
  return MAX_ILDMODSTAT_STR + MAX_TREE_STR + 4;
}


