//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mcs-lock.h>
#include "interval_t.h"

static char
Interval_MaxSpaces[] = "                                               ";


int
interval_t_cmp(void* lhs, void* rhs)
{
  interval_t* p1 = (interval_t*)lhs;
  interval_t* p2 = (interval_t*)rhs;
  if (p1->start < p2->start) return -1;
  if (p1->start == p2->start) return 0;
  return 1;
}

int
interval_t_inrange(void* lhs, void* val)
{
  interval_t* interval = (interval_t*)lhs;
  uintptr_t address = (uintptr_t)val;
  // special boundary case:
  if (address == UINTPTR_MAX && interval->start == UINTPTR_MAX) {
	return 0;
  }
  else {
	return (address < interval->start)? 1: (address < interval->end)? 0: -1;
  }
}

/*
 * pre-condition: result[] is an array of length >= MAX_PTRINTERVAL_STR
 * post-condition: a string of the form [ptrinterval->start, ptrinterval->end)
 */
void
interval_t_tostr(void* ptrinterval, char result[])
{
  interval_t* ptr_interval = (interval_t*)ptrinterval;
  snprintf(result, MAX_INTERVAL_STR, "%s%18p%s%18p%s",
		   "[", (void*)ptr_interval->start, " ... ", (void*)ptr_interval->end, ")");
}

void
interval_t_print(interval_t* ptrinterval)
{
  char ptrinterval_buff[MAX_INTERVAL_STR];
  interval_t_tostr(ptrinterval, ptrinterval_buff);
  printf("%s", ptrinterval_buff);
}

char*
interval_maxspaces()
{
  return Interval_MaxSpaces;
}
