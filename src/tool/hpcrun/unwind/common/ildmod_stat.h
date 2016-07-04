/*
 * ildmod_stat.h
 *
 * Pair of interval_ldmod_pair_t and tree_stat_t.
 * (([s, e), lm), stat)
 *
 *      Author: dxnguyen
 */

#ifndef __ILDMOD_STAT_H__
#define __ILDMOD_STAT_H__

//******************************************************************************
// local include files
//******************************************************************************

#include "interval_ldmod_pair.h"


//******************************************************************************
// macro
//******************************************************************************

#define MAX_STAT_STR 14
#define MAX_ILDMODSTAT_STR MAX_ILDMODPAIR_STR+MAX_STAT_STR

//******************************************************************************
// type
//******************************************************************************

// Tree status
typedef enum {
  NEVER, DEFERRED, FORTHCOMING, READY
} tree_stat_t;

// {<interval, load_module>, tree status}
typedef struct ildmod_stat_s {
  interval_ldmod_pair_t* ildmod;
  volatile tree_stat_t stat;
} ildmod_stat_t;

//******************************************************************************
// Constructors
//******************************************************************************

ildmod_stat_t*
ildmod_stat_new(interval_ldmod_pair_t *key,  tree_stat_t treestat,
	mem_alloc m_alloc);

ildmod_stat_t*
ildmod_stat_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, mem_alloc m_alloc);

//******************************************************************************
// Gettors
//******************************************************************************

inline interval_t*
ildmod_stat_interval(ildmod_stat_t* itp)
{
  return (interval_t*)(itp->ildmod->first);
}

inline load_module_t*
ildmod_stat_loadmod(ildmod_stat_t* itp)
{
  return (load_module_t*)(itp->ildmod->second);
}

//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: ilms1, ilms2 are ildmod_stat_t*
 */
inline int
ildmod_stat_cmp(void *ilms1, void *ilms2)
{
  return interval_ldmod_pair_cmp(((ildmod_stat_t*)ilms1)->ildmod,
	  ((ildmod_stat_t*)ilms2)->ildmod);
}

/*
 * pre-condition: ilms1, ilms2 are ildmod_stat_t*
 */
inline int
ildmod_stat_inrange(void *ilms, void *address)
{
  return interval_ldmod_pair_inrange(((ildmod_stat_t*)ilms)->ildmod, address);
}

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: ilms is of type ildmod_stat_t*
 */
void
ildmod_stat_tostr(void* ilms, char str[]);

void
ildmod_stat_print(void* ilms);

void
treestat_tostr(tree_stat_t stat, char str[]);

void
treestat_print(tree_stat_t ts);

/*
 * the max spaces occupied by "([start_address ... end_address), load module xx, status)
 */
char*
ildmod_stat_maxspaces();

#endif /* __ILDMOD_STAT_H__ */
