// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef UNWINR_INFO_H
#define UNWINR_INFO_H

//************************* System Include Files ****************************

#include <inttypes.h>

//*************************** User Include Files ****************************

#include "binarytree_uwi.h"
#include "../../loadmap.h"
#include "interval_t.h"

//******************************************************************************
// type
//******************************************************************************

/**
 * So far, we have used NEVER to indicate regions of the address space
 * where we cannot compute unwind intervals.
 * Regions outside the segments where code is mapped are marked NEVER.
 *
 * Similarly, if we can’t compute function bounds for a segment (i.e., VDSO),
 * we will also create a region marked NEVER.
 * The reason we use NEVER is that we don’t want to continue trying to
 * build function bounds or intervals for a region in the address space
 * where it will never succeed.
 *
 * When we want to enter unwind intervals for a function into the cskiplist,
 * we insert a record into the cskiplist that says
 *
 *  stat: DEFERRED
 *  bounds [lower, upper)
 *
 *    The DEFERRED status means that no unwind intervals are currently available
 *    for the function range and no one is computing them.
 *
 *    In the code below, any thread that comes along and wants the unwind
 *    intervals for a region that is DEFERRED offers to compute them by
 *    executing a compare-and-swap (CAS) to change DEFERRED to FORTHCOMING.
 *    Multiple threads may try at once. Only one CAS will succeed and
 *    the “winner” with the successful CAS will build the intervals.
 *    All other threads that need them will wait until the FORTHCOMING
 *    intervals are computed, added to the skiplist node and published by
 *    marking the node READY.
*/

// Tree status
typedef enum {
  NEVER, DEFERRED, FORTHCOMING, READY
} tree_stat_t;

typedef struct unwindr_info_s {
  interval_t interval;
  load_module_t *lm;
  tree_stat_t treestat;
  bitree_uwi_t *btuwi;
} unwindr_info_t;


#endif
