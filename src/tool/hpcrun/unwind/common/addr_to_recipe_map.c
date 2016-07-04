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
#include "cskiplist_ilmstat_btuwi_pair.h"

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

  cskl_ilmstat_btuwi_init(SKIPLIST_HEIGHT, m_alloc);
}


// returns an empty cskl_ilmstat_btuwi_t*, cskiplist of ilmstat_btuwi_pair_t*
addr_to_recipe_map_t*
a2r_map_new(mem_alloc m_alloc)
{
  cskl_ilmstat_btuwi_t *map = cskl_ilmstat_btuwi_new(SKIPLIST_HEIGHT, m_alloc);
  return (addr_to_recipe_map_t *)map;
}

/*
 * if the given address, value, is in the range of a node in the cskiplist,
 * return that node, otherwise return NULL.
 */
ilmstat_btuwi_pair_t*
a2r_map_inrange_find(addr_to_recipe_map_t *map, uintptr_t addr)
{
  return cskl_ilmstat_btuwi_inrange_find((cskl_ilmstat_btuwi_t*) map, addr);
}

bool
a2r_map_insert(addr_to_recipe_map_t *map, ilmstat_btuwi_pair_t* itp,
	mem_alloc m_alloc)
{
  return cskl_ilmstat_btuwi_insert((cskl_ilmstat_btuwi_t*) map, itp, m_alloc);
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

bool
a2r_map_cmp_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	ilmstat_btuwi_pair_t* lo,
	ilmstat_btuwi_pair_t* hi)
{
  return cskl_ilmstat_btuwi_cmp_del_bulk_unsynch((cskl_ilmstat_btuwi_t*) map, lo, hi);
}

bool
a2r_map_inrange_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	uintptr_t lo,
	uintptr_t hi)
{
  return cskl_ilmstat_btuwi_inrange_del_bulk_unsynch((cskl_ilmstat_btuwi_t*) map, lo, hi);
}

/*
 * Compute a string representation of map and store result in str.
 */
void
a2r_map_tostring(addr_to_recipe_map_t *map, char str[])
{
  cskl_ilmstat_btuwi_tostring((cskl_ilmstat_btuwi_t*) map, str);
}


void
a2r_map_print(addr_to_recipe_map_t *map)
{
  cskl_ilmstat_btuwi_print((cskl_ilmstat_btuwi_t*)map);
}

