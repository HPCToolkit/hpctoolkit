/*
 * addr_to_recipe_map.c
 *
 * address to recipe map implemented as a cskiplist whose node values are of type
 * ilmstat_btuwi_pair_t*.
 * ilmstat_btuwi_pair_t is a <key, value> pair of the form
 *   <interval_load_module/tree status, binary tree of unwind intervals)
 *
 *      Author: dxnguyen
 */

#define A2R_MAP_DEBUG 0

//******************************************************************************
// local include files
//******************************************************************************

#include "addr_to_recipe_map.h"
#include <lib/prof-lean/cskiplist.h>

//******************************************************************************
// macro
//******************************************************************************

#define SKIPLIST_HEIGHT 8

//******************************************************************************
// interface operations
//******************************************************************************

void
a2r_map_init(mem_alloc m_alloc)
{
#if A2R_MAP_DEBUG
  printf("DXN_DBG a2r_map_init: call cskl_ilmstat_btuwi_init(SKIPLIST_HEIGHT, m_alloc)... \n");
#endif

  cskl_init();
  ilmstat_btuwi_pair_init();
}


// returns an empty cskl_ilmstat_btuwi_t*, cskiplist of ilmstat_btuwi_pair_t*
addr_to_recipe_map_t*
a2r_map_new(mem_alloc m_alloc)
{
  ilmstat_btuwi_pair_t* lsentinel =
	  ilmstat_btuwi_pair_build(0, 0, NULL, NEVER, NULL, m_alloc );
  ilmstat_btuwi_pair_t* rsentinel =
	  ilmstat_btuwi_pair_build(UINTPTR_MAX, UINTPTR_MAX, NULL, NEVER, NULL, m_alloc );
  cskiplist_t *cskl =
	  cskl_new( lsentinel, rsentinel, SKIPLIST_HEIGHT,
		  ilmstat_btuwi_pair_cmp, ilmstat_btuwi_pair_inrange, m_alloc);
  return (addr_to_recipe_map_t *)cskl;
}

/*
 * if the given address, value, is in the range of a node in the cskiplist,
 * return that node, otherwise return NULL.
 */
ilmstat_btuwi_pair_t*
a2r_map_inrange_find(addr_to_recipe_map_t *map, uintptr_t addr)
{
  return (ilmstat_btuwi_pair_t*)cskl_inrange_find((cskiplist_t*)map, (void*)addr);
}

bool
a2r_map_insert(addr_to_recipe_map_t *map, ilmstat_btuwi_pair_t* itp,
	mem_alloc m_alloc)
{
  return cskl_insert((cskiplist_t*) map, itp, m_alloc);
}


/*
 * if the given address, value, is in the range of a node in the cskiplist,
 * return the root node of the binary tree of unwind intervals whose interval
 * key contains the given address,
 * otherwise return NULL.
 */
bitree_uwi_t*
a2r_map_get_btuwi(addr_to_recipe_map_t *map, uintptr_t addr )
{
  ilmstat_btuwi_pair_t *itp = a2r_map_inrange_find(map, addr);
  return itp? ilmstat_btuwi_pair_find(*itp, addr): NULL;
}

uw_recipe_t*
a2r_map_get_recipe(addr_to_recipe_map_t *map, uintptr_t addr )
{
  ilmstat_btuwi_pair_t *itp = a2r_map_inrange_find(map, addr);
  return ilmstat_btuwi_pair_recipe(itp, addr);
}

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

bool
a2r_map_cmp_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	ilmstat_btuwi_pair_t* lo,
	ilmstat_btuwi_pair_t* hi)
{
  return cskl_cmp_del_bulk_unsynch((cskiplist_t*) map, lo, hi, cskl_ilmstat_btuwi_free);
}

bool
a2r_map_inrange_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	uintptr_t lo,
	uintptr_t hi)
{
  return cskl_inrange_del_bulk_unsynch((cskiplist_t*) map, (void*)lo, (void*)hi, cskl_ilmstat_btuwi_free);
}

/*
 * Compute a string representation of map and store result in str.
 */
/*
 * pre-condition: *nodeval is an ilmstat_btuwi_pair_t.
 */
static void
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
a2r_map_tostring(addr_to_recipe_map_t *map, char str[])
{
  cskl_tostr((cskiplist_t*) map, cskl_ilmstat_btuwi_node_tostr, str, MAX_CSKIPLIST_STR);
}


void
a2r_map_print(addr_to_recipe_map_t *map)
{
  char buf[MAX_CSKIPLIST_STR];
  cskl_tostr((cskiplist_t*) map, cskl_ilmstat_btuwi_node_tostr, buf, MAX_CSKIPLIST_STR);
  printf("%s", buf);
}

