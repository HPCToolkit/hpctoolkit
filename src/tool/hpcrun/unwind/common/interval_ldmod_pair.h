/*
 * interval_ldmod_pair.h
 *
 * A generic pair whose first is an interval_t* and whose second is a load_module_t*.
 *
 *      Author: dxnguyen
 */

#ifndef __INTERVAL_LDMOD_PAIR_H__
#define __INTERVAL_LDMOD_PAIR_H__

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/generic_pair.h>
#include <lib/prof-lean/mem_manager.h>
#include <hpcrun/loadmap.h>
#include "interval_t.h"

//******************************************************************************
// macro
//******************************************************************************

#define LDMOD_NAME_LEN 128
#define MAX_ILDMODPAIR_STR MAX_INTERVAL_STR+LDMOD_NAME_LEN

//******************************************************************************
// type
//******************************************************************************

typedef generic_pair_t interval_ldmod_pair_t;


//******************************************************************************
// Constructors
//******************************************************************************

interval_ldmod_pair_t*
interval_ldmod_pair_new(interval_t *intvl,  load_module_t *ldmod,
	mem_alloc m_alloc);

interval_ldmod_pair_t*
interval_ldmod_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	mem_alloc m_alloc);


//******************************************************************************
// Gettors
//******************************************************************************

inline interval_t*
interval_ldmod_pair_interval(interval_ldmod_pair_t* ilmp)
{
  return (interval_t*) ilmp->first;
}

inline load_module_t*
interval_ldmod_pair_loadmod(interval_ldmod_pair_t* ilmp)
{
  return (load_module_t*) ilmp->second;
}

//******************************************************************************
// Settor
//******************************************************************************

inline void
interval_ldmod_pair_set_loadmod(interval_ldmod_pair_t* ilmp, load_module_t *ldmod)
{
  ilmp->second = ldmod;
}

//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: ilmp1, ilmp2 are interval_ldmod_pair_t*
 * return interval_t_cmp(ilmp1->first, ilmp2->first)
 */
int
interval_ldmod_pair_cmp(void *ilmp1, void *ilmp2);

/*
 * pre-condition: ilmp is a interval_ldmod_pair_t*, address is a uintptr_t
 * return interval_t_inrange(ilmp->first, address)
 */
int
interval_ldmod_pair_inrange(void *ilmp, void *address);

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: ilmp is of type interval_ldmod_pair_t*
 */
void
interval_ldmod_pair_tostr(void* ilmp, char str[]);

void
interval_ldmod_pair_print(void* ilmp);

void
load_module_tostr(void* lm, char str[]);

void
load_module_print(void* lm);

/*
 * the max spaces occupied by "([start_address ... end_address), load module xx)
 */
char*
interval_ldmod_maxspaces();


#endif /* __INTERVAL_LDMOD_PAIR_H__ */
