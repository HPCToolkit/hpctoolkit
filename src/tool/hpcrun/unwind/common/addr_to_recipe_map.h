/*
 * addr_to_recipe_map.h
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

#include <lib/prof-lean/mem_manager.h>
#include "uw_recipe.h"
#include "ildmod_btuwi_pair.h"

//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct addr_to_recipe_map_s addr_to_recipe_map_t;


//******************************************************************************
// interface operations
//******************************************************************************

addr_to_recipe_map_t*
a2r_map_new(mem_alloc m_alloc);

ildmod_btuwi_pair_t*
a2r_map_inrange_find(addr_to_recipe_map_t *map, uintptr_t value);

bool
a2r_map_insert(addr_to_recipe_map_t *map, ildmod_btuwi_pair_t* value, mem_alloc m_alloc);

bitree_uwi_t*
a2r_map_get_btuwi(addr_to_recipe_map_t *map, uintptr_t addr );

uw_recipe_t*
a2r_map_get_recipe(addr_to_recipe_map_t *map, uintptr_t addr );

bool
a2r_map_inrange_del_bulk_unsynch(addr_to_recipe_map_t *map,
	uintptr_t* low, uintptr_t* high, mem_free m_free);

bool
a2r_map_cmp_del_bulk_unsynch(addr_to_recipe_map_t *map,
	ildmod_btuwi_pair_t* low, ildmod_btuwi_pair_t* high, mem_free m_free);

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
