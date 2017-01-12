/*
 * interval_ldmod_pair.c
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

#include "interval_ldmod_pair.h"

//******************************************************************************
// Gettors (inline)
//******************************************************************************

extern interval_t* interval_ldmod_pair_interval(interval_ldmod_pair_t* itp);
extern load_module_t* interval_ldmod_pair_loadmod(interval_ldmod_pair_t* itp);

//******************************************************************************
// Constructors
//******************************************************************************

interval_ldmod_pair_t*
interval_ldmod_pair_new(interval_t *intvl,  load_module_t *ldmod,
	mem_alloc m_alloc)
{
  return generic_pair_t_new(intvl, ldmod, m_alloc);
}

interval_ldmod_pair_t*
interval_ldmod_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	mem_alloc m_alloc)
{
  interval_t* intv = interval_t_new(start, end, m_alloc);
  return interval_ldmod_pair_new(intv, ldmod, m_alloc);
}

//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: ilmp1, ilmp2 are interval_ldmod_pair_t*
 * return ptrinterval_cmp(ilmp1->first, ilmp2->first)
 */
int
interval_ldmod_pair_cmp(void *lhs, void *rhs)
{
  interval_ldmod_pair_t* itp1 = (interval_ldmod_pair_t*)lhs;
  interval_ldmod_pair_t* itp2 = (interval_ldmod_pair_t*)rhs;
  return interval_t_cmp(itp1->first, itp2->first);
}

/*
 * pre-condition: ilmp is a interval_ldmod_pair_t*, address is a uintptr_t
 * return interval_t_inrange(ilmp->first, address)
 */
int
interval_ldmod_pair_inrange(void *lhs, void *address)
{
  interval_ldmod_pair_t* ilmp = (interval_ldmod_pair_t*)lhs;
  return interval_t_inrange(ilmp->first, address);
}

//******************************************************************************
// String output
//******************************************************************************

void
load_module_tostr(void* lm, char str[])
{
  load_module_t* ldmod = (load_module_t*)lm;
  if (ldmod) {
	snprintf(str, LDMOD_NAME_LEN, "%s%s%d", ldmod->name, " ", ldmod->id);
  }
  else {
	snprintf(str, LDMOD_NAME_LEN, "%s", "nil");
  }
}

void
load_module_print(void* lm)
{
  char str[LDMOD_NAME_LEN];
  load_module_tostr(lm, str);
  printf("%s", str);
}

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: ilmp is of type interval_ldmod_pair_t*
 */
void
interval_ldmod_pair_tostr(void* ilmp, char str[])
{
  char firststr[MAX_INTERVAL_STR];
  char secondstr[LDMOD_NAME_LEN];
  generic_pair_t_tostr(ilmp, interval_t_tostr, firststr,
	  load_module_tostr, secondstr, str);
}

void
interval_ldmod_pair_print(void* ilmp)
{
  char str[MAX_ILDMODPAIR_STR];
  interval_ldmod_pair_tostr(ilmp, str);
  printf("%s", str);
}
