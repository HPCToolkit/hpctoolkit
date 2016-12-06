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
// Copyright ((c)) 2002-2015, Rice University
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

/*
 * Interval tree code specific to unwind intervals.
 * This is the interface to the unwind interval tree.
 *
 * Note: the external functions assume the tree is not yet locked.
 *
 * $Id$
 */

//---------------------------------------------------------------------
// system include files
//---------------------------------------------------------------------

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>



//---------------------------------------------------------------------
// local include files
//---------------------------------------------------------------------
#include <memory/hpcrun-malloc.h>
#include "fnbounds_interface.h"
#include "thread_data.h"
#include "ui_tree.h"
#include "hpcrun_stats.h"
#include "addr_to_recipe_map.h"

#include <loadmap.h>
#include <messages/messages.h>

// libmonitor functions
#include <monitor.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>
#include <lib/prof-lean/mcs-lock.h>



//---------------------------------------------------------------------
// macros
//---------------------------------------------------------------------

#define UITREE_DEBUG 0

#ifdef NONZERO_THRESHOLD
#define DEADLOCK_DEFAULT 5000
#else  // ! NONZERO_THRESHOLD ==> DEADLOCK_DEFAULT = 0
#define DEADLOCK_DEFAULT 0
#endif // NONZERO_THRESHOLD



//---------------------------------------------------------------------
// local data
//---------------------------------------------------------------------

static size_t iter_count = 0;

// The concrete representation of the abstract data type unwind recipe map.
static addr_to_recipe_map_t *addr2recipe_map = NULL;

// memory allocator for creating addr2recipe_map
// and inserting entries into addr2recipe_map:
static mem_alloc my_alloc = hpcrun_malloc;



//---------------------------------------------------------------------
// external declarations 
//---------------------------------------------------------------------

extern btuwi_status_t 
build_intervals(char *ins, unsigned int len, mem_alloc m_alloc);



//---------------------------------------------------------------------
// forward declarations 
//---------------------------------------------------------------------

static ilmstat_btuwi_pair_t *
uw_recipe_map_lookup_ilmstat_btuwi_pair_helper(void *addr);

static ilmstat_btuwi_pair_t * 
uw_recipe_map_lookup_ilmstat_btuwi_pair(void *addr);



//---------------------------------------------------------------------
// private operations
//---------------------------------------------------------------------

static bool
uw_recipe_map_poison(uintptr_t start, uintptr_t end)
{
  ilmstat_btuwi_pair_t* itpair =
	  ilmstat_btuwi_pair_build(start, end, NULL, NEVER, NULL, my_alloc);
  return a2r_map_insert(addr2recipe_map, itpair, my_alloc);
}


static bool
uw_recipe_map_unpoison(uintptr_t start, uintptr_t end)
{
  ilmstat_btuwi_pair_t* ilmstat_btuwi =	a2r_map_inrange_find(addr2recipe_map, start);

#if UITREE_DEBUG
  assert(ilmstat_btuwi != NULL); // start should be in range of some poisoned interval
#endif

#if UITREE_DEBUG
  ildmod_stat_t* ilmstat = ilmstat_btuwi_pair_ilmstat(ilmstat_btuwi);
  assert(ilmstat->stat == NEVER);  // should be a poisoned node
#endif
  interval_t* interval = ilmstat_btuwi_pair_interval(ilmstat_btuwi);
  uintptr_t s0 = interval->start;
  uintptr_t e0 = interval->end;
  a2r_map_cmp_del_bulk_unsynch(addr2recipe_map, ilmstat_btuwi, ilmstat_btuwi);
  uw_recipe_map_poison(s0, start);
  return uw_recipe_map_poison(end, e0);
}


static bool
uw_recipe_map_repoison(uintptr_t start, uintptr_t end)
{
  if (start > 0) {
    ilmstat_btuwi_pair_t* itp_left = a2r_map_inrange_find(addr2recipe_map, start - 1);
    if (itp_left) { // poisoned interval on the left
            interval_t* interval_left = ilmstat_btuwi_pair_interval(itp_left);
            start = interval_left->start;
            a2r_map_cmp_del_bulk_unsynch(addr2recipe_map, itp_left, itp_left);
    }
  }
  if (end < UINTPTR_MAX) {
    ilmstat_btuwi_pair_t* itp_right = a2r_map_inrange_find(addr2recipe_map, end);
    if (itp_right) { // poisoned interval on the right
            interval_t* interval_right = ilmstat_btuwi_pair_interval(itp_right);
            end = interval_right->start;
            a2r_map_cmp_del_bulk_unsynch(addr2recipe_map, itp_right, itp_right);
    }
  }
  return uw_recipe_map_poison(start,end);
}


static ilmstat_btuwi_pair_t *
uw_recipe_map_lookup_ilmstat_btuwi_pair_helper(void *addr) {
  // first check if addr is already in the range of an interval key in the map
  ilmstat_btuwi_pair_t* ilmstat_btuwi =
	  a2r_map_inrange_find(addr2recipe_map, (uintptr_t)addr);

  if (!ilmstat_btuwi) {
	load_module_t *lm;
	void *fcn_start, *fcn_end;
	bool ret = fnbounds_enclosing_addr(addr, &fcn_start, &fcn_end, &lm);
	if (!ret) {
	  TMSG(UITREE, "BAD fnbounds_enclosing_addr failed: addr %p", addr);
	  return (NULL);
	}
	if (addr < fcn_start || fcn_end <= addr) {
	  TMSG(UITREE, "BAD fnbounds_enclosing_addr failed: addr %p "
		  "not within fcn range %p to %p", addr, fcn_start, fcn_end);
	  return (NULL);
	}

	// bounding addresses found; set DEFERRED state and pair it with
	// (bitree_uwi_t*)NULL and try to insert into map:
	ilmstat_btuwi =
		ilmstat_btuwi_pair_malloc((uintptr_t)fcn_start, (uintptr_t)fcn_end, lm,
			DEFERRED, NULL, my_alloc);

	bool inserted =  a2r_map_insert(addr2recipe_map, ilmstat_btuwi, my_alloc);
	// if successful: ilmstat_btuwi is now in the map.

	if (!inserted) {
		// interval_ldmod_pair ([fcn_start, fcn_end), lm) is already in the map,
	    // so free it:
	  ilmstat_btuwi_pair_free(ilmstat_btuwi);

	  // look for it in the map and it should be there:
	  ilmstat_btuwi =
	  	  a2r_map_inrange_find(addr2recipe_map, (uintptr_t)addr);
	}
  }
#if UITREE_DEBUG
  assert(ilmstat_btuwi != NULL);
#endif
  return ilmstat_btuwi;
}


/*
 * Remove intervals in the range [start, end) from the unwind interval
 * tree.
 */
static void
uw_recipe_map_delete_range(void* start, void* end)
{
  //  use EMSG to log this call.
  EMSG("uw_recipe_map_delete_range from %p to %p \n", start, end);
  a2r_map_inrange_del_bulk_unsynch(addr2recipe_map, (uintptr_t)start, (uintptr_t)end - 1);
}


static void
uw_recipe_map_notify_map(void *start, void *end)
{
   uw_recipe_map_unpoison((uintptr_t)start, (uintptr_t)end);
}


static void
uw_recipe_map_notify_unmap(void *start, void *end)
{
  uw_recipe_map_delete_range(start, end);

  // join poisoned intervals here.
  uw_recipe_map_repoison((uintptr_t)start, (uintptr_t)end);
}

static void
uw_recipe_map_notify_init()
{
   static loadmap_notify_t uw_recipe_map_notifiers;

   uw_recipe_map_notifiers.map = uw_recipe_map_notify_map;
   uw_recipe_map_notifiers.unmap = uw_recipe_map_notify_unmap;
   hpcrun_loadmap_notify_register(&uw_recipe_map_notifiers);
}



//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------

void
uw_recipe_map_init(void)
{

#if UITREE_DEBUG
  printf("DXN_DBG uw_recipe_map_init: call a2r_map_init(my_alloc) ... \n");
#endif

  a2r_map_init(my_alloc);

  TMSG(UITREE, "init address-to-recipe map");
  addr2recipe_map = a2r_map_new(my_alloc);
  
  uw_recipe_map_notify_init();

  // initialize the map with a POISONED node ({([0, UINTPTR_MAX), NULL), NEVER}, NULL)
  uw_recipe_map_poison(0, UINTPTR_MAX);

  iter_count = DEADLOCK_DEFAULT;
  if (getenv("HPCRUN_DEADLOCK_THRESHOLD")) {
	iter_count = atoi(getenv("HPCRUN_DEADLOCK_THRESHOLD"));
	if (iter_count < 0) iter_count = 0;
  }
  TMSG(DEADLOCK, "deadlock threshold set to %d", iter_count);
}


/*
 *
 */
bool
uw_recipe_map_lookup(void *addr, unwindr_info_t *unwr_info)
{
  ilmstat_btuwi_pair_t *ilm_btui = uw_recipe_map_lookup_ilmstat_btuwi_pair(addr);

  if (ilm_btui == NULL) {  // lookup fails for various reasons.
	// fill unwr_info with appropriate values to indicate that the lookup fails and the unwind recipe
	// information is invalid.
	// known use case:
	// hpcrun_generate_backtrace_no_trampoline calls
	//   1. hpcrun_unw_init_cursor(&cursor, context), which calls
	//        uw_recipe_map_lookup,
	//   2. hpcrun_unw_step(&cursor), which calls
	//        hpcrun_unw_step_real(cursor), which looks at cursor->unwr_info

	unwr_info->btuwi    = NULL;
	unwr_info->treestat = NEVER;
	unwr_info->lm       = NULL;
	unwr_info->start    = 0;
	unwr_info->end      = 0;
	return false;
  }

  bitree_uwi_t *btuwi = ilmstat_btuwi_pair_btuwi(ilm_btui);
  unwr_info->btuwi    = bitree_uwi_inrange(btuwi, (uintptr_t)addr);
  unwr_info->treestat = ilmstat_btuwi_pair_stat(ilm_btui);
  unwr_info->lm       = ilmstat_btuwi_pair_loadmod(ilm_btui);
  unwr_info->start    = ilmstat_btuwi_pair_interval(ilm_btui)->start;
  unwr_info->end      = ilmstat_btuwi_pair_interval(ilm_btui)->end;

  return (unwr_info->treestat == READY);
}

/*
 *
 */
static ilmstat_btuwi_pair_t * uw_recipe_map_lookup_ilmstat_btuwi_pair(void *addr)
{
  ilmstat_btuwi_pair_t* ilmstat_btuwi =
	  uw_recipe_map_lookup_ilmstat_btuwi_pair_helper(addr);
  if (!ilmstat_btuwi)
	return NULL;

  ildmod_stat_t *ilmstat = ilmstat_btuwi_pair_ilmstat(ilmstat_btuwi);
  switch(ilmstat->stat) {
  case NEVER:
	// addr is in the range of some poisoned load module
	return NULL;
  case DEFERRED:
	if (compare_and_swap_bool(&(ilmstat->stat), DEFERRED, FORTHCOMING)) {
	  // it is my responsibility to build the tree of intervals for the function
	  void *fcn_start = (void*)interval_ldmod_pair_interval(ilmstat->ildmod)->start;
	  void *fcn_end   = (void*)interval_ldmod_pair_interval(ilmstat->ildmod)->end;
	  btuwi_status_t btuwi_stat = build_intervals(fcn_start, fcn_end - fcn_start, my_alloc);
	  if (btuwi_stat.first == NULL) {
		TMSG(UITREE, "BAD build_intervals failed: fcn range %p to %p",
			fcn_start, fcn_end);
		return (NULL);
	  }
	  ilmstat_btuwi_pair_set_btuwi(ilmstat_btuwi, bitree_uwi_rebalance(btuwi_stat.first));
	  ilmstat_btuwi_pair_set_status(ilmstat_btuwi, READY);
	}
	break;
  case READY:
	break;
  case FORTHCOMING:
	// invariant: ilmstat_btuwi is non-null
	while(ilmstat_btuwi_pair_stat(ilmstat_btuwi) != READY);
	break;
  }

  TMSG(UITREE_LOOKUP, "found in unwind tree: addr %p", addr);
  return ilmstat_btuwi;
}

//---------------------------------------------------------------------
// debug operations
//---------------------------------------------------------------------

void
uw_recipe_map_print(void)
{
  a2r_map_print(addr2recipe_map);
}

