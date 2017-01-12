/*
 * uwi.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "uwi.h"

extern interval_t* uwi_t_interval(uwi_t* uwi);
extern uw_recipe_t* uwi_t_recipe(uwi_t* uwi);

uwi_t*
uwi_t_new(interval_t* key, uw_recipe_t* val, mem_alloc m_alloc)
{
  return generic_pair_t_new(key, val, m_alloc);
}

int
uwi_t_cmp(void* lhs, void* rhs)
{
  uwi_t* uwi1 = (uwi_t*)lhs;
  uwi_t* uwi2 = (uwi_t*)rhs;
  return interval_t_cmp(uwi1->first, uwi2->first);
}

int
uwi_t_inrange(void* lhs, void* address)
{
  uwi_t* uwi = (uwi_t*)lhs;
  return interval_t_inrange(uwi->first, address);
}

//******************************************************************************
// String output
//******************************************************************************

void
uwi_t_tostr(void* uwi, char str[])
{
  char firststr[MAX_INTERVAL_STR];
  char secondstr[MAX_RECIPE_STR];
  generic_pair_t_tostr(uwi, interval_t_tostr, firststr,
						uw_recipe_tostr, secondstr, str);
}

void
uwi_t_print(uwi_t* uwi) {
  char str[MAX_UWI_STR];
  uwi_t_tostr(uwi, str);
  printf("%s", str);
}
