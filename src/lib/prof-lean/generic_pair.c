/*
 * generic_pair.c
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
#include "generic_pair.h"


generic_pair_t*
generic_pair_t_new(void* first, void* second, mem_alloc m_alloc)
{
  generic_pair_t* genpair = (generic_pair_t*)m_alloc(sizeof(generic_pair_t));
  genpair->first  = first;
  genpair->second = second;
  return genpair;
}

void
generic_pair_t_tostr(generic_pair_t* gen_pair,
					  val_tostr first_tostr, char firststr[],
					  val_tostr second_tostr, char secondstr[], char str[])
{
  if (gen_pair) {
	first_tostr(gen_pair->first, firststr);
	second_tostr(gen_pair->second, secondstr);
	snprintf(str, strlen(firststr) + strlen(secondstr) + 6, "%s%s%s%s%s",
		"(", firststr, ",  ", secondstr, ")");
  }
  else {
	strcpy(str, "");
  }
}
