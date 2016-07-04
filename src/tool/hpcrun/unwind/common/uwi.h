/*
 * uwi.h
 *
 * uwi_t is a <key, value> pair whose key is an interval,
 * interval_t, as specified in interval.h, and whose
 * value is an unwind recipe, recipe_t, as specified in uw_recipe.h.
 *
 *  uwi_t is implemented as a generic pair, generic_pair_t, as
 *  defined in generic_pair.h, whose first is a interval_t* serving
 *  as the key and whose second is a uw_recipe_t* serving as the value.
 *
 *      Author: dxnguyen
 */

#ifndef __UNWIND_INTERVAL_H__
#define __UNWIND_INTERVAL_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdint.h>


//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/generic_pair.h>
#include <lib/prof-lean/mem_manager.h>
#include "interval_t.h"
#include "uw_recipe.h"

//******************************************************************************
// macro
//******************************************************************************

#define MAX_UWI_STR MAX_INTERVAL_STR+MAX_RECIPE_STR+4


//******************************************************************************
// type
//******************************************************************************

typedef generic_pair_t uwi_t;


//******************************************************************************
// Constructor
//******************************************************************************

uwi_t*
uwi_t_new(interval_t* key, uw_recipe_t* val, mem_alloc m_alloc);

//******************************************************************************
// Gettors
//******************************************************************************

inline interval_t*
uwi_t_interval(uwi_t* uwi)
{
  return (interval_t*) uwi->first;
}

inline uw_recipe_t*
uwi_t_recipe(uwi_t* uwi)
{
  return (uw_recipe_t*) uwi->second;
}

//******************************************************************************
// Comparison
//******************************************************************************

/*
 * pre-condition: uwi1, uwi2 are uwi_t*
 * return ptrinterval_cmp(uwi1.first, uwi2.first)
 */
int
uwi_t_cmp(void* uwi1, void* uwi2);

/*
 * pre-condition: uwi is a uwi_t*, address is a uintptr_t
 * check to see if the given address is in the range of the interval given by
 * uwi->first.
 * return interval_inrange(uwi->first, address)
 */
int
uwi_t_inrange(void* uwi, void* address);

//******************************************************************************
// String output
//******************************************************************************

void
uwi_t_tostr(void* uwi, char str[]);

void
uwi_t_print(uwi_t* uwi);

#endif /* __UNWIND_INTERVAL_H__ */
