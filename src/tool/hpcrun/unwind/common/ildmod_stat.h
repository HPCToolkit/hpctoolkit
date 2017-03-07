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

#include <lib/prof-lean/generic_pair.h>
#include <lib/prof-lean/mem_manager.h>
#include <hpcrun/loadmap.h>
#include "interval_t.h"


//******************************************************************************
// macro
//******************************************************************************

#define MAX_STAT_STR 14
#define LDMOD_NAME_LEN 128
#define MAX_ILDMODSTAT_STR MAX_INTERVAL_STR+LDMOD_NAME_LEN+MAX_STAT_STR

//******************************************************************************
// type
//******************************************************************************

// Tree status
typedef enum {
  NEVER, DEFERRED, FORTHCOMING, READY
} tree_stat_t;

// {interval, load_module, tree status}
typedef struct ildmod_stat_s {
  interval_t *interval;
  load_module_t *loadmod;
  _Atomic(tree_stat_t) stat;
} ildmod_stat_t;

//******************************************************************************
// String output
//******************************************************************************

/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the generic_val class.
 *  pre-condition: ilmp is of type interval_ldmod_pair_t*
 */

void
load_module_tostr(void* lm, char str[]);

void
load_module_print(void* lm);

/*
 * the max spaces occupied by "([start_address ... end_address), load module xx)
 */
char*
interval_ldmod_maxspaces();


/*
 *  Concrete implementation of the abstract function val_tostr
 *  of the ildmod_stat class.
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
