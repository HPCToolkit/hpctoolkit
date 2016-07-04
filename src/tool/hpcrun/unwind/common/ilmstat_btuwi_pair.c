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
#include <assert.h>

#define ILMSTAT_BTUWI_DEBUG 0
#define NUM_NODES 10

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mcs-lock.h>
#include <lib/prof-lean/binarytree.h>
#include "ilmstat_btuwi_pair.h"

static ilmstat_btuwi_pair_t *GF_ilmstat_btuwi = NULL; // global free list of ilmstat_btuwi_pair_t*
static mcs_lock_t GFL_lock;  // lock for GF_ilmstat_btuwi
static __thread  ilmstat_btuwi_pair_t *_lf_ilmstat_btuwi = NULL;  // thread local free list of ilmstat_btuwi_pair_t*


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
extern void ilmstat_btuwi_pair_set_loadmod(ilmstat_btuwi_pair_t* itp, load_module_t *lmod);


//******************************************************************************
// Constructors
//******************************************************************************

extern ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, bitree_uwi_t *tree,	mem_alloc m_alloc);

ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_new(ildmod_stat_t *key,  bitree_uwi_t *tree,
	mem_alloc m_alloc)
{
  ilmstat_btuwi_pair_t* node = (ilmstat_btuwi_pair_t*)m_alloc(sizeof(ilmstat_btuwi_pair_t));
  node->next = NULL;
  node->ilmstat_btuwi = generic_pair_t_new(key, tree, m_alloc);
  return node;
}

/*
 * add a bunch of nodes to _lf_ilmstat_btuwi
 */
static void
ilmstat_btuwi_pair_add_nodes_to_lfl(
	mem_alloc m_alloc)
{
	for (int i = 0; i < NUM_NODES; i++) {
	  ilmstat_btuwi_pair_t *node =
		  ilmstat_btuwi_pair_build(0, 0, NULL, DEFERRED, NULL, m_alloc);
	  node->next = _lf_ilmstat_btuwi;
	  _lf_ilmstat_btuwi = node;
	}
}

/*
 * remove the head node of the local free list
 * set its fields using the appropriate parameters
 * return the head node.
 */
static ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_alloc_from_lfl(
	uintptr_t start,
	uintptr_t end,
	load_module_t *ldmod,
	tree_stat_t treestat,
	bitree_uwi_t *tree)
{
  ilmstat_btuwi_pair_t *ans = _lf_ilmstat_btuwi;
  _lf_ilmstat_btuwi = _lf_ilmstat_btuwi->next;
  ans->next = NULL;
  interval_t *interval = ilmstat_btuwi_pair_interval(ans);
  interval->start = start;
  interval->end = end;
  ilmstat_btuwi_pair_set_loadmod(ans, ldmod);
  ilmstat_btuwi_pair_set_status(ans, treestat);
  ilmstat_btuwi_pair_set_btuwi(ans, tree);
  return ans;
}

/*
 * Look for nodes in the global free list.
 * If the global free list is not empty,
 *   transfer a bunch of nodes to the local free list,
 * otherwise add a bunch of nodes to the local free list.
 */
static void
ilmstat_btuwi_pair_populate_lfl(
	mem_alloc m_alloc)
{
  mcs_node_t me;
  bool acquired = mcs_trylock(&GFL_lock, &me);
  if (acquired) {
	if (GF_ilmstat_btuwi) {
	  // transfer a bunch of nodes the global free list to the local free list
	  int n = 0;
	  while (GF_ilmstat_btuwi && n < NUM_NODES) {
		ilmstat_btuwi_pair_t *head = GF_ilmstat_btuwi;
		GF_ilmstat_btuwi = GF_ilmstat_btuwi->next;
		head->next = _lf_ilmstat_btuwi;
		_lf_ilmstat_btuwi = head;
		n++;
	  }
	}
	mcs_unlock(&GFL_lock, &me);
  }
  if (!_lf_ilmstat_btuwi) {
	ilmstat_btuwi_pair_add_nodes_to_lfl(m_alloc);
  }
}

ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_malloc(
	uintptr_t start,
	uintptr_t end,
	load_module_t *ldmod,
	tree_stat_t treestat,
	bitree_uwi_t *tree,
	mem_alloc m_alloc)
{
  if (!_lf_ilmstat_btuwi) {
	ilmstat_btuwi_pair_populate_lfl(m_alloc);
  }

#if ILMSTAT_BTUWI_DEBUG
  assert (_lf_ilmstat_btuwi);
#endif

  return ilmstat_btuwi_pair_alloc_from_lfl(start, end, ldmod, treestat, tree);
}

void
ilmstat_btuwi_pair_init()
{
#if ILMSTAT_BTUWI_DEBUG
  printf("ilmstat_btuwi_pair_init: mcs_init(&GFL_lock), call bitree_uwi_init() \n");
  fflush(stdout);
#endif
  mcs_init(&GFL_lock);
  bitree_uwi_init();
}

//******************************************************************************
// Destructor
//******************************************************************************

void
ilmstat_btuwi_pair_free(ilmstat_btuwi_pair_t* pair)
{
  if (!pair) return;

  bitree_uwi_t *btuwi = ilmstat_btuwi_pair_btuwi(pair);
  ilmstat_btuwi_pair_set_btuwi(pair, NULL);
  bitree_uwi_free(btuwi);

  // add pair to the front of the  global free list of ilmstat_btuwi_pair_t*:
  mcs_node_t me;
  mcs_lock(&GFL_lock, &me);
  pair->next = GF_ilmstat_btuwi;
  GF_ilmstat_btuwi = pair;
  mcs_unlock(&GFL_lock, &me);
}

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
  ildmod_stat_t *ilms = ilmstat_btuwi_pair_ilmstat(it_pair);
  bitree_uwi_t *tree = ilmstat_btuwi_pair_btuwi(it_pair);
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


