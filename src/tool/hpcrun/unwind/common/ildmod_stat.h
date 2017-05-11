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

#include <hpcrun/loadmap.h>
#include "interval_t.h"

//******************************************************************************
// type
//******************************************************************************

// Tree status
typedef enum {
  NEVER, DEFERRED, FORTHCOMING, READY
} tree_stat_t;

// {interval, load_module, tree status}
typedef struct ildmod_stat_s {
  interval_t interval;
  load_module_t *loadmod;
  _Atomic(tree_stat_t) stat;
} ildmod_stat_t;

#endif /* __ILDMOD_STAT_H__ */
