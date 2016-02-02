/*
 * ildmod_btuwi_pair.c
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
#include "ildmod_btuwi_pair.h"

extern interval_ldmod_pair_t*
ildmod_btuwi_pair_ildmod(ildmod_btuwi_pair_t* itp);

extern bitree_uwi_t*
ildmod_btuwi_pair_btuwi(ildmod_btuwi_pair_t* itp);

extern void
ildmod_btuwi_pair_set_btuwi(ildmod_btuwi_pair_t* itp, bitree_uwi_t* tree);

interval_t*
ildmod_btuwi_pair_interval(ildmod_btuwi_pair_t* itp)
{
  return interval_ldmod_pair_interval(ildmod_btuwi_pair_ildmod(itp));
}

load_module_t*
ildmod_btuwi_pair_loadmod(ildmod_btuwi_pair_t* itp)
{
  return interval_ldmod_pair_loadmod(ildmod_btuwi_pair_ildmod(itp));
}

uw_recipe_t*
ildmod_btuwi_pair_recipe(ildmod_btuwi_pair_t* itp, uintptr_t addr )
{
  if (itp) {
	bitree_uwi_t *found =
		bitree_uwi_inrange(ildmod_btuwi_pair_btuwi(itp), addr);
	return found? bitree_uwi_recipe(found): NULL;
  }
  return NULL;
}

ildmod_btuwi_pair_t*
ildmod_btuwi_pair_t_new(interval_ldmod_pair_t *key,  bitree_uwi_t *tree,
	mem_alloc m_alloc)
{
  return generic_pair_t_new(key, tree, m_alloc);
}

ildmod_btuwi_pair_t*
ildmod_btuwi_pair_t_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	bitree_uwi_t *tree,	mem_alloc m_alloc)
{
  interval_ldmod_pair_t* intv =
	  interval_ldmod_pair_build(start, end, ldmod, m_alloc);
  return ildmod_btuwi_pair_t_new(intv, tree, m_alloc);
}

int
ildmod_btuwi_pair_cmp(void *lhs, void *rhs)
{
  ildmod_btuwi_pair_t* itp1 = (ildmod_btuwi_pair_t*)lhs;
  ildmod_btuwi_pair_t* itp2 = (ildmod_btuwi_pair_t*)rhs;
  return interval_ldmod_pair_cmp(itp1->first, itp2->first);
}

int
ildmod_btuwi_pair_inrange(void *lhs, void *address)
{
  ildmod_btuwi_pair_t* itp = (ildmod_btuwi_pair_t*)lhs;
  return interval_ldmod_pair_inrange(itp->first, address);
}

bitree_uwi_t*
ildmod_btuwi_pair_find(ildmod_btuwi_pair_t itp, uintptr_t address)
{
  bitree_uwi_t *tree =  (bitree_uwi_t*)itp.second;  // a binary search tree of uwi_t
  return bitree_uwi_inrange(tree, address);
}


//******************************************************************************
// String output
//******************************************************************************

void
ildmod_btuwi_pair_tostr(void *itp, char str[])
{
  ildmod_btuwi_pair_tostr_indent(itp, interval_ldmod_maxspaces(), str);
}

void
ildmod_btuwi_pair_tostr_indent(void *itp, char* indents, char str[])
{
  ildmod_btuwi_pair_t* it_pair = (ildmod_btuwi_pair_t*)itp;
  interval_ldmod_pair_t *interval = (interval_ldmod_pair_t*)it_pair->first;
  bitree_uwi_t *tree = (bitree_uwi_t*)it_pair->second;
  char firststr[MAX_INTERVAL_STR];
  char secondstr[MAX_TREE_STR];
  interval_ldmod_pair_t_tostr(interval, firststr);
  bitree_uwi_tostring_indent(tree, indents, secondstr);
  snprintf(str, strlen(firststr) + strlen(secondstr) + 5, "%s%s%s%s%s",
		   "(", firststr, ",  ", secondstr, ")");
}

void
ildmod_btuwi_pair_print(ildmod_btuwi_pair_t *itp)
{
  char str[MAX_ILDMODPAIR_STR + MAX_TREE_STR + 4];
  ildmod_btuwi_pair_tostr(itp, str);
  printf("%s", str);
}

int
max_ildmod_btuwi_pair_len()
{
  return MAX_ILDMODPAIR_STR + MAX_TREE_STR + 4;
}
