/*
 * cskiplist_ilmstat_btuwi_pair.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <string.h>
#include <stdio.h>

#define CSKL_ILS_BTU 0

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mcs-lock.h>
#include <lib/prof-lean/cskiplist.h>
#include "cskiplist_ilmstat_btuwi_pair.h"

static void
cskl_ilmstat_btuwi_free(void *anode)
{
#if CSKL_ILS_BTU
//  printf("DXN_DBG cskl_ilmstat_btuwi_free(%p)...\n", anode);
#endif

  csklnode_t *node = (csklnode_t*) anode;
  ilmstat_btuwi_pair_t *ilmstat_btuwi = (ilmstat_btuwi_pair_t*)node->val;
  ilmstat_btuwi_pair_free(ilmstat_btuwi);
  node->val = NULL;
  cskl_free(node);
}

//******************************************************************************
// interface operations
//******************************************************************************

void
cskl_ilmstat_btuwi_init(int maxheight, mem_alloc m_alloc)
{
#if CSKL_ILS_BTU
  printf("DXN_DBG cskl_ilmstat_btuwi_init: call cskl_init(), ilmstat_btuwi_pair_init()...\n");
#endif
  cskl_init();
  ilmstat_btuwi_pair_init();
}


cskl_ilmstat_btuwi_t *
cskl_ilmstat_btuwi_new(int maxheight, mem_alloc m_alloc)
{
  ilmstat_btuwi_pair_t* lsentinel =
	  ilmstat_btuwi_pair_build(0, 0, NULL, NEVER, NULL, m_alloc );
  ilmstat_btuwi_pair_t* rsentinel =
	  ilmstat_btuwi_pair_build(UINTPTR_MAX, UINTPTR_MAX, NULL, NEVER, NULL, m_alloc );
  cskiplist_t *cskl =
	  cskl_new( lsentinel, rsentinel, maxheight,
		  ilmstat_btuwi_pair_cmp, ilmstat_btuwi_pair_inrange, m_alloc);
  return (cskl_ilmstat_btuwi_t*)cskl;

}

ilmstat_btuwi_pair_t*
cskl_ilmstat_btuwi_cmp_find(cskl_ilmstat_btuwi_t *cskl, ilmstat_btuwi_pair_t* value)
{
  return (ilmstat_btuwi_pair_t*)cskl_cmp_find((cskiplist_t*)cskl, value);
}

/*
 * if the given address, value, is in the range of a node in the cskiplist,
 * return that node, otherwise return NULL.
 */
ilmstat_btuwi_pair_t*
cskl_ilmstat_btuwi_inrange_find(cskl_ilmstat_btuwi_t *cskl, uintptr_t value)
{
  return (ilmstat_btuwi_pair_t*)cskl_inrange_find((cskiplist_t*)cskl, (void*)value);
}

bool
cskl_ilmstat_btuwi_insert(cskl_ilmstat_btuwi_t *cskl, ilmstat_btuwi_pair_t* value, mem_alloc m_alloc)
{
  return cskl_insert((cskiplist_t *)cskl, value, m_alloc);
}

bool
cskl_ilmstat_btuwi_cmp_del_bulk_unsynch(
	cskl_ilmstat_btuwi_t *cskl,
	ilmstat_btuwi_pair_t* low,
	ilmstat_btuwi_pair_t* high)
{
  return cskl_cmp_del_bulk_unsynch((cskiplist_t *)cskl, low, high, cskl_ilmstat_btuwi_free);
}

bool
cskl_ilmstat_btuwi_inrange_del_bulk_unsynch(
	cskl_ilmstat_btuwi_t *cskl,
	uintptr_t low,
	uintptr_t high)
{
  return cskl_inrange_del_bulk_unsynch((cskiplist_t *)cskl, (void*)low, (void*)high, cskl_ilmstat_btuwi_free);
}

/*
 * pre-condition: *nodeval is an ilmstat_btuwi_pair_t.
 */
void
cskl_ilmstat_btuwi_node_tostr(void* nodeval, int node_height, int max_height,
	char str[], int max_cskl_str_len)
{
  cskl_levels_tostr(node_height, max_height, str, max_cskl_str_len);

  // build needed indentation to print the binary tree inside the skiplist:
  char cskl_itpair_treeIndent[MAX_CSKIPLIST_STR];
  cskl_itpair_treeIndent[0] = '\0';
  int indentlen= strlen(cskl_itpair_treeIndent);
  strncat(cskl_itpair_treeIndent, str, MAX_CSKIPLIST_STR - indentlen -1);
  indentlen= strlen(cskl_itpair_treeIndent);
  strncat(cskl_itpair_treeIndent, ildmod_stat_maxspaces(), MAX_CSKIPLIST_STR - indentlen -1);

  // print the binary tree with the proper indentation:
  char itpairstr[max_ilmstat_btuwi_pair_len()];
  ilmstat_btuwi_pair_t* node_val = (ilmstat_btuwi_pair_t*)nodeval;
  ilmstat_btuwi_pair_tostr_indent(node_val, cskl_itpair_treeIndent, itpairstr);

  // add new line:
  cskl_append_node_str(itpairstr, str, max_cskl_str_len);
}

void
cskl_ilmstat_btuwi_tostring(cskl_ilmstat_btuwi_t *cskl, char str[])
{
  cskl_tostr((cskiplist_t *)cskl, cskl_ilmstat_btuwi_node_tostr, str, MAX_CSKIPLIST_STR);
}

void
cskl_ilmstat_btuwi_print(cskl_ilmstat_btuwi_t *cskl)
{
  char cskl_buff[MAX_CSKIPLIST_STR];
  cskl_ilmstat_btuwi_tostring(cskl, cskl_buff);
  printf("%s", cskl_buff);
}

