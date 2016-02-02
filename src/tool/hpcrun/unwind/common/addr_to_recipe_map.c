/*
 * addr_to_recipe_map.c
 *
 * addr_to_recipe_map implemented as a cskiplist of interval_tree_pair.
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// local include files
//******************************************************************************

#include "addr_to_recipe_map.h"
#include "cskiplist_ildmod_btuwi_pair.h"

//******************************************************************************
// macro
//******************************************************************************

#define SKIPLIST_HEIGHT 8

//******************************************************************************
// interface operations
//******************************************************************************

addr_to_recipe_map_t*
a2r_map_new(mem_alloc m_alloc)
{
  cskl_ildmod_btuwi_t *map = cskl_ildmod_btuwi_new(SKIPLIST_HEIGHT, m_alloc);
  return (addr_to_recipe_map_t *)map;
}

ildmod_btuwi_pair_t*
a2r_map_inrange_find(addr_to_recipe_map_t *map, uintptr_t addr)
{
  return cskl_ildmod_btuwi_inrange_find((cskl_ildmod_btuwi_t*) map, addr);
}

bool
a2r_map_insert(addr_to_recipe_map_t *map, ildmod_btuwi_pair_t* itp,
	mem_alloc m_alloc)
{
  return cskl_ildmod_btuwi_insert((cskl_ildmod_btuwi_t*) map, itp, m_alloc);
}

bitree_uwi_t*
a2r_map_get_btuwi(addr_to_recipe_map_t *map, uintptr_t addr )
{
  ildmod_btuwi_pair_t *itp = a2r_map_inrange_find(map, addr);
  return itp? ildmod_btuwi_pair_find(*itp, addr): NULL;
}

uw_recipe_t*
a2r_map_get_recipe(addr_to_recipe_map_t *map, uintptr_t addr )
{
  ildmod_btuwi_pair_t *itp = a2r_map_inrange_find(map, addr);
  return ildmod_btuwi_pair_recipe(itp, addr);
}

bool
a2r_map_cmp_del_bulk_unsynch(addr_to_recipe_map_t *map,
	ildmod_btuwi_pair_t* lo, ildmod_btuwi_pair_t* hi, mem_free m_free)
{
  return cskl_ildmod_btuwi_cmp_del_bulk_unsynch((cskl_ildmod_btuwi_t*) map, lo, hi, m_free);
}

bool
a2r_map_inrange_del_bulk_unsynch(addr_to_recipe_map_t *map,
	uintptr_t* lo, uintptr_t* hi, mem_free m_free)
{
  return cskl_ildmod_btuwi_inrange_del_bulk_unsynch((cskl_ildmod_btuwi_t*) map, lo, hi, m_free);
}

/*
 * Compute a string representation of map and store result in str.
 */
void
a2r_map_tostring(addr_to_recipe_map_t *map, char str[])
{
  cskl_ildmod_btuwi_tostring((cskl_ildmod_btuwi_t*) map, str);
}


void
a2r_map_print(addr_to_recipe_map_t *map)
{
  cskl_ildmod_btuwi_print((cskl_ildmod_btuwi_t*)map);
}

