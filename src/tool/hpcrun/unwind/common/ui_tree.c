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
// Copyright ((c)) 2002-2017, Rice University
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
#include <lib/prof-lean/cskiplist.h>

#include <loadmap.h>
#include <messages/messages.h>

// libmonitor functions
#include <monitor.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/mcs-lock.h>



//---------------------------------------------------------------------
// macros
//---------------------------------------------------------------------

#define SKIPLIST_HEIGHT 8

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
static cskiplist_t *addr2recipe_map = NULL;

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
  return cskl_insert(addr2recipe_map, itpair, my_alloc);
}


/*
 * if the given address, value, is in the range of a node in the cskiplist,
 * return that node, otherwise return NULL.
 */
static ilmstat_btuwi_pair_t*
uw_recipe_map_inrange_find(uintptr_t addr)
{
  return (ilmstat_btuwi_pair_t*)cskl_inrange_find(addr2recipe_map, (void*)addr);
}

static void
cskl_ilmstat_btuwi_free(void *anode)
{
#if CSKL_ILS_BTU
//  printf("DXN_DBG cskl_ilmstat_btuwi_free(%p)...\n", anode);
#endif

  csklnode_t *node = (csklnode_t*) anode;
  ilmstat_btuwi_pair_t *ilmstat_btuwi = (ilmstat_btuwi_pair_t*)node->val;
  ilmstat_btuwi_pair_free(ilmstat_btuwi);
  node->val = NULL;
  cskl_free(node);
}

static bool
uw_recipe_map_cmp_del_bulk_unsynch(
	ilmstat_btuwi_pair_t* lo,
	ilmstat_btuwi_pair_t* hi)
{
  return cskl_cmp_del_bulk_unsynch(addr2recipe_map, lo, hi, cskl_ilmstat_btuwi_free);
}

static bool
uw_recipe_map_unpoison(uintptr_t start, uintptr_t end)
{
  ilmstat_btuwi_pair_t* ilmstat_btuwi =	uw_recipe_map_inrange_find(start);

#if UITREE_DEBUG
  assert(ilmstat_btuwi != NULL); // start should be in range of some poisoned interval
#endif

#if UITREE_DEBUG
  ildmod_stat_t* ilmstat = ilmstat_btuwi_pair_ilmstat(ilmstat_btuwi);
  assert(atomic_load_explicit(&ilmstat->stat, memory_order_relaxed) == NEVER);  // should be a poisoned node
#endif
  interval_t* interval = ilmstat_btuwi_pair_interval(ilmstat_btuwi);
  uintptr_t s0 = interval->start;
  uintptr_t e0 = interval->end;
  uw_recipe_map_cmp_del_bulk_unsynch(ilmstat_btuwi, ilmstat_btuwi);
  uw_recipe_map_poison(s0, start);
  return uw_recipe_map_poison(end, e0);
}


static bool
uw_recipe_map_repoison(uintptr_t start, uintptr_t end)
{
  if (start > 0) {
    ilmstat_btuwi_pair_t* itp_left = uw_recipe_map_inrange_find(start - 1);
    if (itp_left) { // poisoned interval on the left
            interval_t* interval_left = ilmstat_btuwi_pair_interval(itp_left);
            start = interval_left->start;
            uw_recipe_map_cmp_del_bulk_unsynch(itp_left, itp_left);
    }
  }
  if (end < UINTPTR_MAX) {
    ilmstat_btuwi_pair_t* itp_right = uw_recipe_map_inrange_find(end);
    if (itp_right) { // poisoned interval on the right
            interval_t* interval_right = ilmstat_btuwi_pair_interval(itp_right);
            end = interval_right->start;
            uw_recipe_map_cmp_del_bulk_unsynch(itp_right, itp_right);
    }
  }
  return uw_recipe_map_poison(start,end);
}


static ilmstat_btuwi_pair_t *
uw_recipe_map_lookup_ilmstat_btuwi_pair_helper(void *addr) {
  // first check if addr is already in the range of an interval key in the map
  ilmstat_btuwi_pair_t* ilmstat_btuwi =
	  uw_recipe_map_inrange_find((uintptr_t)addr);

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

	bool inserted =  cskl_insert(addr2recipe_map, ilmstat_btuwi, my_alloc);
	// if successful: ilmstat_btuwi is now in the map.

	if (!inserted) {
		// interval_ldmod_pair ([fcn_start, fcn_end), lm) is already in the map,
	    // so free it:
	  ilmstat_btuwi_pair_free(ilmstat_btuwi);

	  // look for it in the map and it should be there:
	  ilmstat_btuwi =
	  	  uw_recipe_map_inrange_find((uintptr_t)addr);
	}
  }
#if UITREE_DEBUG
  assert(ilmstat_btuwi != NULL);
#endif
  return ilmstat_btuwi;
}


static void
uw_recipe_map_notify_map(void *start, void *end)
{
   uw_recipe_map_unpoison((uintptr_t)start, (uintptr_t)end);
}


static void
uw_recipe_map_notify_unmap(void *start, void *end)
{
  // Remove intervals in the range [start, end) from the unwind interval tree.
  TMSG(UITREE, "uw_recipe_map_delete_range from %p to %p", start, end);
  cskl_inrange_del_bulk_unsynch(addr2recipe_map, start, ((void*)(intptr_t)end - 1), cskl_ilmstat_btuwi_free);

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

  cskl_init();
  ilmstat_btuwi_pair_init();

  TMSG(UITREE, "init address-to-recipe map");
  ilmstat_btuwi_pair_t* lsentinel =
	  ilmstat_btuwi_pair_build(0, 0, NULL, NEVER, NULL, my_alloc );
  ilmstat_btuwi_pair_t* rsentinel =
	  ilmstat_btuwi_pair_build(UINTPTR_MAX, UINTPTR_MAX, NULL, NEVER, NULL, my_alloc );
  addr2recipe_map =
	  cskl_new( lsentinel, rsentinel, SKIPLIST_HEIGHT,
		  ilmstat_btuwi_pair_cmp, ilmstat_btuwi_pair_inrange, my_alloc);

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
  tree_stat_t deferred = DEFERRED;
  switch(atomic_load_explicit(&ilmstat->stat, memory_order_relaxed)) {
  case NEVER:
	// addr is in the range of some poisoned load module
	return NULL;
  case DEFERRED:
      if (atomic_compare_exchange_strong_explicit(&ilmstat->stat, &deferred, FORTHCOMING,
						  memory_order_relaxed, memory_order_relaxed)) {
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

/*
 * Compute a string representation of map and store result in str.
 */
/*
 * pre-condition: *nodeval is an ilmstat_btuwi_pair_t.
 */

static void
cskl_ilmstat_btuwi_node_tostr(void* nodeval, int node_height, int max_height,
	char str[], int max_cskl_str_len)
{
  cskl_levels_tostr(node_height, max_height, str, max_cskl_str_len);

  // build needed indentation to print the binary tree inside the skiplist:
  char cskl_itpair_treeIndent[MAX_CSKIPLIST_STR];
  cskl_itpair_treeIndent[0] = '\0';
  int indentlen= strlen(cskl_itpair_treeIndent);
  strncat(cskl_itpair_treeIndent, str, MAX_CSKIPLIST_STR - indentlen -1);
  indentlen= strlen(cskl_itpair_treeIndent);
  strncat(cskl_itpair_treeIndent, ildmod_stat_maxspaces(), MAX_CSKIPLIST_STR - indentlen -1);

  // print the binary tree with the proper indentation:
  char itpairstr[max_ilmstat_btuwi_pair_len()];
  ilmstat_btuwi_pair_t* node_val = (ilmstat_btuwi_pair_t*)nodeval;
  ilmstat_btuwi_pair_tostr_indent(node_val, cskl_itpair_treeIndent, itpairstr);

  // add new line:
  cskl_append_node_str(itpairstr, str, max_cskl_str_len);
}

void
uw_recipe_map_print(void)
{
  char buf[MAX_CSKIPLIST_STR];
  cskl_tostr(addr2recipe_map, cskl_ilmstat_btuwi_node_tostr, buf, MAX_CSKIPLIST_STR);
  printf("%s", buf);
}

