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
 * Maintain a map from address Intervals to unwind intervals.
 *
 * Note: the caller need not acquire/release locks as part
 * of using the map.
 *
 * $Id$
 */

//---------------------------------------------------------------------
// debugging support
//---------------------------------------------------------------------

#define UW_RECIPE_MAP_DEBUG 0

#define UW_RECIPE_MAP_DEBUG_VERBOSE 0

#if UW_RECIPE_MAP_DEBUG_VERBOSE
#undef UW_RECIPE_MAP_DEBUG
#define UW_RECIPE_MAP_DEBUG 1
#endif


#if UW_RECIPE_MAP_DEBUG
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#endif



//---------------------------------------------------------------------
// local include files
//---------------------------------------------------------------------
#include <memory/hpcrun-malloc.h>
#include <main.h>
#include "uw_recipe_map.h"
#include "unwind-interval.h"
#include <fnbounds/fnbounds_interface.h>
#include <lib/prof-lean/cskiplist.h>
#include <lib/prof-lean/mcs-lock.h>
#include <lib/prof-lean/binarytree.h>
#include "ildmod_stat.h"
#include "binarytree_uwi.h"
#include "segv_handler.h"
#include <messages/messages.h>

// libmonitor functions
#include <monitor.h>

/*
 * ilmstat_btuwi_pair.c
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//---------------------------------------------------------------------
// macros
//---------------------------------------------------------------------

#define SKIPLIST_HEIGHT 8

#define NUM_NODES 10

/*
 * A <key, value> pair, whose key is a ildmod_stat_t* and whose value is a bitree_uwi_t.
 *   ildmod_stat_t (interval-load_module/stat) is a struct whose first component is
 *     an interval_ldmod_pair_t* and whose second component is a tree_stat_t (tree status)
 *   bitree_uwi_t (binary tree of unwind intervals) is binary search tree of unwind
 *     intervals uwi_t*.
 *     uwi_t is a <key, value> pair whose key is an interval interval_t, and whose
 *     value is an unwind recipe, recipe_t, as specified in uw_recipe.h.
 *
 *      Author: dxnguyen
 */

//******************************************************************************
// type
//******************************************************************************

typedef struct ilmstat_btuwi_pair_s {
  ildmod_stat_t *ilmstat;
  bitree_uwi_t *btuwi;
} ilmstat_btuwi_pair_t;

//******************************************************************************
// Comparators
//******************************************************************************

/*
 * pre-condition: lhs, rhs are ilmstat_btuwi_pair_t*
 */
int
ilmstat_btuwi_pair_cmp(void *lhs, void *rhs)
{
  ilmstat_btuwi_pair_t *l = (ilmstat_btuwi_pair_t*)lhs;
  ilmstat_btuwi_pair_t *r = (ilmstat_btuwi_pair_t*)rhs;
  return interval_t_cmp(
	 &l->ilmstat->interval,
	 &r->ilmstat->interval);
}

/*
 * pre-condition: itp is a ilmstat_btuwi_pair_t*, address is a uintptr_t
 * return interval_ldmod_pair_inrange(itp->first, address)
 */
extern int
ilmstat_btuwi_pair_inrange(void *itp, void *address)
{
  ilmstat_btuwi_pair_t *p = (ilmstat_btuwi_pair_t*)itp;
  return interval_t_inrange(&p->ilmstat->interval, address);
}

static ilmstat_btuwi_pair_t *GF_ilmstat_btuwi = NULL; // global free list of ilmstat_btuwi_pair_t*
static mcs_lock_t GFL_lock;  // lock for GF_ilmstat_btuwi
static __thread  ilmstat_btuwi_pair_t *_lf_ilmstat_btuwi = NULL;  // thread local free list of ilmstat_btuwi_pair_t*
// for storing the current btuwi in case of segv
static  __thread  ilmstat_btuwi_pair_t *current_btuwi = NULL;

//******************************************************************************
// Constructors
//******************************************************************************

static inline ilmstat_btuwi_pair_t *
ilmstat__btuwi_pair_init(ilmstat_btuwi_pair_t *node,
			 tree_stat_t treestat,
			 load_module_t *ldmod,
			 uintptr_t start,
			 uintptr_t end,
			 bitree_uwi_t *tree)
{
  ildmod_stat_t *ilmstat = node->ilmstat;
  atomic_store_explicit(&ilmstat->stat, treestat, memory_order_relaxed);
  ilmstat->loadmod = ldmod;
  ilmstat->interval.start = start;
  ilmstat->interval.end = end;
  node->btuwi = tree;
  return node;
}

static inline ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_build(uintptr_t start, uintptr_t end, load_module_t *ldmod,
	tree_stat_t treestat, bitree_uwi_t *tree,	mem_alloc m_alloc)
{
  ilmstat_btuwi_pair_t* node = m_alloc(sizeof(*node));
  node->ilmstat = m_alloc(sizeof(*node->ilmstat));
  return ilmstat__btuwi_pair_init(node, treestat, ldmod, start, end, tree);
}

static inline void
push_free_pair(ilmstat_btuwi_pair_t **list, ilmstat_btuwi_pair_t *pair)
{
  pair->btuwi = (bitree_uwi_t *)*list;
  *list = pair;
}

static inline ilmstat_btuwi_pair_t *
pop_free_pair(ilmstat_btuwi_pair_t **list)
{
  ilmstat_btuwi_pair_t *head = *list;
  *list = (ilmstat_btuwi_pair_t *)head->btuwi;
  return head;
}

static ilmstat_btuwi_pair_t*
ilmstat_btuwi_pair_malloc(
	uintptr_t start,
	uintptr_t end,
	load_module_t *ldmod,
	tree_stat_t treestat,
	bitree_uwi_t *tree,
	mem_alloc m_alloc)
{
  if (!_lf_ilmstat_btuwi) {
    /*
     * Look for nodes in the global free list.
     * If the global free list is not empty,
     *   transfer a bunch of nodes to the local free list,
     * otherwise add a bunch of nodes to the local free list.
     */
    mcs_node_t me;
    bool acquired = mcs_trylock(&GFL_lock, &me);
    if (acquired) {
      if (GF_ilmstat_btuwi) {
	// transfer a bunch of nodes the global free list to the local free list
	int n = 0;
	while (GF_ilmstat_btuwi && n < NUM_NODES) {
	  ilmstat_btuwi_pair_t *head = pop_free_pair(&GF_ilmstat_btuwi);
	  push_free_pair(&_lf_ilmstat_btuwi, head);
	  n++;
	}
      }
      mcs_unlock(&GFL_lock, &me);
    }
    if (!_lf_ilmstat_btuwi) {
      /* add a bunch of nodes to _lf_ilmstat_btuwi */
      for (int i = 0; i < NUM_NODES; i++) {
	ilmstat_btuwi_pair_t *node =
	  ilmstat_btuwi_pair_build(0, 0, NULL, DEFERRED, NULL, m_alloc);
	push_free_pair(&_lf_ilmstat_btuwi, node);
      }
    }
  }

#if UW_RECIPE_MAP_DEBUG
  assert (_lf_ilmstat_btuwi);
#endif

/*
 * remove the head node of the local free list
 * set its fields using the appropriate parameters
 * return the head node.
 */
  ilmstat_btuwi_pair_t *ans = pop_free_pair(&_lf_ilmstat_btuwi);
  return ilmstat__btuwi_pair_init(ans, treestat, ldmod, start, end, tree);
}

//******************************************************************************
// Destructor
//******************************************************************************

static void
ilmstat_btuwi_pair_free(ilmstat_btuwi_pair_t* pair)
{
  if (!pair) return;

  bitree_uwi_t *btuwi = pair->btuwi;
  pair->btuwi = NULL;
  bitree_uwi_free(btuwi);

  // add pair to the front of the  global free list of ilmstat_btuwi_pair_t*:
  mcs_node_t me;
  mcs_lock(&GFL_lock, &me);
  push_free_pair(&GF_ilmstat_btuwi, pair);
  mcs_unlock(&GFL_lock, &me);
}

//---------------------------------------------------------------------
// local data
//---------------------------------------------------------------------

// The concrete representation of the abstract data type unwind recipe map.
static cskiplist_t *addr2recipe_map = NULL;

// memory allocator for creating addr2recipe_map
// and inserting entries into addr2recipe_map:
static mem_alloc my_alloc = hpcrun_malloc;

//******************************************************************************
// String output
//******************************************************************************


#define MAX_STAT_STR 14
#define LDMOD_NAME_LEN 128
#define MAX_ILDMODSTAT_STR MAX_INTERVAL_STR+LDMOD_NAME_LEN+MAX_STAT_STR

static void
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

static void
treestat_tostr(tree_stat_t stat, char str[])
{
  switch (stat) {
  case NEVER: strcpy(str, "      NEVER"); break;
  case DEFERRED: strcpy(str, "   DEFERRED"); break;
  case FORTHCOMING: strcpy(str, "FORTHCOMING"); break;
  case READY: strcpy(str, "      READY"); break;
  default: strcpy(str, "STAT_ERROR");
  }
}

static void
ildmod_stat_tostr(void* ilms, char str[])
{
  ildmod_stat_t* ildmod_stat = (ildmod_stat_t*)ilms;
  char intervalstr[MAX_INTERVAL_STR];
  interval_t_tostr(&ildmod_stat->interval, intervalstr);
  char ldmodstr[LDMOD_NAME_LEN];
  load_module_tostr(ildmod_stat->loadmod, ldmodstr);
  char statstr[MAX_STAT_STR];
  treestat_tostr(atomic_load_explicit(&ildmod_stat->stat, memory_order_relaxed), statstr);
  sprintf(str, "(%s %s %s)", intervalstr, ldmodstr, statstr);
}

/*
 * Compute the string representation of ilmstat_btuwi_pair_t with appropriate
 * indentation of the second component which is a binary tree.
 * Return the result in the parameter str.
 */
static void
ilmstat_btuwi_pair_tostr_indent(void* itp, char* indents, char str[])
{
  ilmstat_btuwi_pair_t* it_pair = (ilmstat_btuwi_pair_t*)itp;
  ildmod_stat_t *ilms = it_pair->ilmstat;
  bitree_uwi_t *tree = it_pair->btuwi;
  char firststr[MAX_ILDMODSTAT_STR];
  char secondstr[MAX_TREE_STR];
  ildmod_stat_tostr(ilms, firststr);
  bitree_uwi_tostring_indent(tree, indents, secondstr);
  snprintf(str, strlen(firststr) + strlen(secondstr) + 6, "%s%s%s%s%s",
	  "(", firststr, ",  ", secondstr, ")");
}

static int
max_ilmstat_btuwi_pair_len()
{
  return MAX_ILDMODSTAT_STR + MAX_TREE_STR + 4;
}


static char
ILdMod_Stat_MaxSpaces[] = "                                                                              ";

/*
 * the max spaces occupied by "([start_address ... end_address), load module xx, status)
 */
static char*
ildmod_stat_maxspaces()
{
  return ILdMod_Stat_MaxSpaces;
}

//---------------------------------------------------------------------
// private operations
//---------------------------------------------------------------------

#if UW_RECIPE_MAP_DEBUG
static void
uw_recipe_map_report(const char *op, void *start, void *end)
{
  fprintf(stderr, "%s [start=%p, end=%p)\n", op, start, end);
}
#else
#define uw_recipe_map_report(op, start, end)
#endif

static void
uw_recipe_map_poison(uintptr_t start, uintptr_t end)
{
  uw_recipe_map_report("uw_recipe_map_poison", (void *) start, (void *) end);

  ilmstat_btuwi_pair_t* itpair =
	  ilmstat_btuwi_pair_build(start, end, NULL, NEVER, NULL, my_alloc);
  csklnode_t *node = cskl_insert(addr2recipe_map, itpair, my_alloc);
  if (itpair != (ilmstat_btuwi_pair_t*)node->val)
    ilmstat_btuwi_pair_free(itpair);
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

static void
uw_recipe_map_unpoison(uintptr_t start, uintptr_t end)
{
  ilmstat_btuwi_pair_t* ilmstat_btuwi =	uw_recipe_map_inrange_find(start);

  uw_recipe_map_report("uw_recipe_map_unpoison", (void *) start, (void *) end);

#if UW_RECIPE_MAP_DEBUG
  assert(ilmstat_btuwi != NULL); // start should be in range of some poisoned interval

  ildmod_stat_t* ilmstat = ilmstat_btuwi->ilmstat;
  assert(ilmstat != NULL); 
  assert(atomic_load_explicit(&ilmstat->stat, memory_order_relaxed) == NEVER);  // should be a poisoned node
#else
  ildmod_stat_t* ilmstat = ilmstat_btuwi->ilmstat;
#endif

  uintptr_t s0 = ilmstat->interval.start;
  uintptr_t e0 = ilmstat->interval.end;
  uw_recipe_map_cmp_del_bulk_unsynch(ilmstat_btuwi, ilmstat_btuwi);
  uw_recipe_map_poison(s0, start);
  uw_recipe_map_poison(end, e0);
}


static void
uw_recipe_map_repoison(uintptr_t start, uintptr_t end)
{
  if (start > 0) {
    ilmstat_btuwi_pair_t* ileft = uw_recipe_map_inrange_find(start - 1);
    if (ileft) { 
      ildmod_stat_t* ilmstat = ileft->ilmstat;
      if ((ilmstat->interval.end == start) &&
          (NEVER == atomic_load_explicit(&ilmstat->stat, memory_order_acquire))) {
        // poisoned interval adjacent on the left
        start = ilmstat->interval.start;
        uw_recipe_map_cmp_del_bulk_unsynch(ileft, ileft);
      }
    }
  }
  if (end < UINTPTR_MAX) {
    ilmstat_btuwi_pair_t* iright = uw_recipe_map_inrange_find(end+1);
    if (iright) { 
      ildmod_stat_t* ilmstat = iright->ilmstat;
      if ((ilmstat->interval.start == end) &&
          (NEVER == atomic_load_explicit(&ilmstat->stat, memory_order_acquire))) {
        // poisoned interval adjacent on the right
        end = ilmstat->interval.end;
        uw_recipe_map_cmp_del_bulk_unsynch(iright, iright);
      }
    }
  }
  uw_recipe_map_poison(start, end);
}


#if UW_RECIPE_MAP_DEBUG_VERBOSE 
static void
uw_recipe_map_report_and_dump(const char *op, void *start, void *end)
{
  uw_recipe_map_report(op, start, end);
  uw_recipe_map_print();
}
#else 
#define uw_recipe_map_report_and_dump(op, start, end)
#endif


#if UW_RECIPE_MAP_DEBUG_VERBOSE 
static void
uw_recipe_map_report_and_dump(const char *op, void *start, void *end)
{
  uw_recipe_map_report(op, start, end);
  uw_recipe_map_print();
}
#else 
#define uw_recipe_map_report_and_dump(op, start, end)
#endif


static void
uw_recipe_map_notify_map(void *start, void *end)
{
  uw_recipe_map_report_and_dump("*** map: before unpoisoning", start, end);

  uw_recipe_map_unpoison((uintptr_t)start, (uintptr_t)end);

  uw_recipe_map_report_and_dump("*** map: after unpoisoning", start, end);
}


static void
uw_recipe_map_notify_unmap(void *start, void *end)
{
  uw_recipe_map_report_and_dump("*** unmap: before poisoning", start, end);

  // Remove intervals in the range [start, end) from the unwind interval tree.
  TMSG(UW_RECIPE_MAP, "uw_recipe_map_delete_range from %p to %p", start, end);
  cskl_inrange_del_bulk_unsynch(addr2recipe_map, start, ((void*)((char *) end) - 1), cskl_ilmstat_btuwi_free);

  // join poisoned intervals here.
  uw_recipe_map_repoison((uintptr_t)start, (uintptr_t)end);

  uw_recipe_map_report_and_dump("*** unmap: after poisoning", start, end);
}

static void
uw_recipe_map_notify_init()
{
   static loadmap_notify_t uw_recipe_map_notifiers;

   uw_recipe_map_notifiers.map = uw_recipe_map_notify_map;
   uw_recipe_map_notifiers.unmap = uw_recipe_map_notify_unmap;
   hpcrun_loadmap_notify_register(&uw_recipe_map_notifiers);
}


/*
 * clean-up the state in case of emergency such as SEGV
 */
static void
uw_cleanup(void)
{
  if (current_btuwi) {
    ildmod_stat_t *ilmstat = current_btuwi->ilmstat;
    atomic_store_explicit(&ilmstat->stat, NEVER, memory_order_release);
  }
}

//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------

void
uw_recipe_map_init(void)
{
  hpcrun_set_real_siglongjmp();
#if UW_RECIPE_MAP_DEBUG
  fprintf(stderr, "uw_recipe_map_init: call a2r_map_init(my_alloc) ... \n");
#endif

  cskl_init();
#if UW_RECIPE_MAP_DEBUG
  fprintf(stderr, "%s: mcs_init(&GFL_lock), call bitree_uwi_init() \n", __func__);
#endif
  mcs_init(&GFL_lock);
  bitree_uwi_init();

  TMSG(UW_RECIPE_MAP, "init address-to-recipe map");
  ilmstat_btuwi_pair_t* lsentinel =
	  ilmstat_btuwi_pair_build(0, 0, NULL, NEVER, NULL, my_alloc );
  ilmstat_btuwi_pair_t* rsentinel =
	  ilmstat_btuwi_pair_build(UINTPTR_MAX, UINTPTR_MAX, NULL, NEVER, NULL, my_alloc );
  addr2recipe_map =
	  cskl_new(lsentinel, rsentinel, SKIPLIST_HEIGHT,
		  ilmstat_btuwi_pair_cmp, ilmstat_btuwi_pair_inrange, my_alloc);

  uw_recipe_map_notify_init();

  // register to segv signal handler to call this function 
  hpcrun_segv_register_cb(uw_cleanup);

  // initialize the map with a POISONED node ({([0, UINTPTR_MAX), NULL), NEVER}, NULL)
  uw_recipe_map_poison(0, UINTPTR_MAX);
}


/*
 *
 */
bool
uw_recipe_map_lookup(void *addr, unwindr_info_t *unwr_info)
{
  // fill unwr_info with appropriate values to indicate that the lookup fails and the unwind recipe
  // information is invalid, in case of failure.
  // known use case:
  // hpcrun_generate_backtrace_no_trampoline calls
  //   1. hpcrun_unw_init_cursor(&cursor, context), which calls
  //        uw_recipe_map_lookup,
  //   2. hpcrun_unw_step(&cursor), which calls
  //        hpcrun_unw_step_real(cursor), which looks at cursor->unwr_info

  unwr_info->btuwi    = NULL;
  unwr_info->treestat = NEVER;
  unwr_info->lm       = NULL;
  unwr_info->interval.start = 0;
  unwr_info->interval.end   = 0;

  // check if addr is already in the range of an interval key in the map
  ilmstat_btuwi_pair_t* ilm_btui =
	  uw_recipe_map_inrange_find((uintptr_t)addr);

  if (!ilm_btui) {
	load_module_t *lm;
	void *fcn_start, *fcn_end;
	if (!fnbounds_enclosing_addr(addr, &fcn_start, &fcn_end, &lm)) {
	  TMSG(UW_RECIPE_MAP, "BAD fnbounds_enclosing_addr failed: addr %p", addr);
	  return false;
	}
	if (addr < fcn_start || fcn_end <= addr) {
	  TMSG(UW_RECIPE_MAP, "BAD fnbounds_enclosing_addr failed: addr %p "
		  "not within fcn range %p to %p", addr, fcn_start, fcn_end);
	  return false;
	}

	// bounding addresses found; set DEFERRED state and pair it with
	// (bitree_uwi_t*)NULL and try to insert into map:
	ilm_btui =
		ilmstat_btuwi_pair_malloc((uintptr_t)fcn_start, (uintptr_t)fcn_end, lm,
			DEFERRED, NULL, my_alloc);

	
	csklnode_t *node = cskl_insert(addr2recipe_map, ilm_btui, my_alloc);
	if (ilm_btui !=  (ilmstat_btuwi_pair_t*)node->val) {
	  // interval_ldmod_pair ([fcn_start, fcn_end), lm) is already in the map,
	  // so free the unused copy and use the mapped one
	  ilmstat_btuwi_pair_free(ilm_btui);
	  ilm_btui = (ilmstat_btuwi_pair_t*)node->val;
	}
	// ilm_btui is now in the map.
  }
#if UW_RECIPE_MAP_DEBUG
  assert(ilm_btui != NULL);
#endif

  ildmod_stat_t *ilmstat = ilm_btui->ilmstat;
  tree_stat_t oldstat = DEFERRED;
  if (atomic_compare_exchange_strong_explicit(&ilmstat->stat, &oldstat, FORTHCOMING,
					      memory_order_release, memory_order_relaxed)) {
    // it is my responsibility to build the tree of intervals for the function
    void *fcn_start = (void*)ilmstat->interval.start;
    void *fcn_end   = (void*)ilmstat->interval.end;

    // potentially crash in this statement. need to save the state 
    current_btuwi = ilm_btui;

    btuwi_status_t btuwi_stat = build_intervals(fcn_start, fcn_end - fcn_start, my_alloc);
    if (btuwi_stat.first == NULL) {
      atomic_store_explicit(&ilmstat->stat, NEVER, memory_order_release);
      TMSG(UW_RECIPE_MAP, "BAD build_intervals failed: fcn range %p to %p",
	   fcn_start, fcn_end);
      return false;
    }
    ilm_btui->btuwi = bitree_uwi_rebalance(btuwi_stat.first, btuwi_stat.count);
    current_btuwi = NULL;
    atomic_store_explicit(&ilmstat->stat, READY, memory_order_release);
  }
  else {
    while (FORTHCOMING == oldstat)
      oldstat = atomic_load_explicit(&ilmstat->stat, memory_order_acquire);
    if (oldstat == NEVER) {
      // addr is in the range of some poisoned load module
      return false;
    }
  }

  TMSG(UW_RECIPE_MAP_LOOKUP, "found in unwind tree: addr %p", addr);

  bitree_uwi_t *btuwi = ilm_btui->btuwi;
  unwr_info->btuwi    = bitree_uwi_inrange(btuwi, (uintptr_t)addr);
  unwr_info->treestat = READY;
  unwr_info->lm         = ilmstat->loadmod;
  unwr_info->interval   = ilmstat->interval;

  return true;
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
  fprintf(stderr, "%s", buf);
}
