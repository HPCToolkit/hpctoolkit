/*
 * addr_to_recipe_map.h
 *
 * address to recipe map.
 *
 *      Author: dxnguyen
 */

#ifndef __ADDR_TO_RECIPE_MAP_H__
#define __ADDR_TO_RECIPE_MAP_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdint.h>


//******************************************************************************
// local include files
//******************************************************************************

#include "ilmstat_btuwi_pair.h"

//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct addr_to_recipe_map_s addr_to_recipe_map_t;


//******************************************************************************
// interface operations
//******************************************************************************

void
a2r_map_init(mem_alloc m_alloc);

/*
 * returns an empty address to recipe map
 */
addr_to_recipe_map_t*
a2r_map_new(mem_alloc m_alloc);


/*
 * if the given address, value, is in the range of a node in the map,
 * return that node, otherwise return NULL.
 */
ilmstat_btuwi_pair_t*
a2r_map_inrange_find(addr_to_recipe_map_t *map, uintptr_t value);

bool
a2r_map_insert(addr_to_recipe_map_t *map, ilmstat_btuwi_pair_t* value, mem_alloc m_alloc);

/*
 * if the given address, value, is in the range of a node in the map,
 * return the root node of the binary tree of unwind intervals whose interval
 * key contains the given address,
 * otherwise return NULL.
 */
bitree_uwi_t*
a2r_map_get_btuwi(addr_to_recipe_map_t *map, uintptr_t addr );

uw_recipe_t*
a2r_map_get_recipe(addr_to_recipe_map_t *map, uintptr_t addr );

bool
a2r_map_inrange_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	uintptr_t low,
	uintptr_t high);

bool
a2r_map_cmp_del_bulk_unsynch(
	addr_to_recipe_map_t *map,
	ilmstat_btuwi_pair_t* low,
	ilmstat_btuwi_pair_t* high);

/*
 * Compute a string representation of map and store result in str.
 */
void
a2r_map_tostring(addr_to_recipe_map_t *map, char str[]);

/*
 * Print string representation of map to standard output.
 */
void
a2r_map_print(addr_to_recipe_map_t *map);


#endif /* __ADDR_TO_RECIPE_MAP_H__ */
