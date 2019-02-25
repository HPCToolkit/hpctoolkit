// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef UNWINR_INFO_H
#define UNWINR_INFO_H

//************************* System Include Files ****************************

#include <inttypes.h>

//*************************** User Include Files ****************************

#include "binarytree_uwi.h"
#include <hpcrun/loadmap.h>
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
