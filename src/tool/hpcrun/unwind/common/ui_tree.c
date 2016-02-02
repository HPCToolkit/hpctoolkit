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
// Copyright ((c)) 2002-2016, Rice University
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

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include <memory/hpcrun-malloc.h>
#include "fnbounds_interface.h"
#include "thread_data.h"
#include "ui_tree.h"
#include "hpcrun_stats.h"
#include "addr_to_recipe_map.h"

#include <messages/messages.h>

// libmonitor functions
#include <monitor.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

// Locks both the UI tree and the UI free list.
spinlock_t ui_tree_lock = SPINLOCK_UNLOCKED;

#define DEADLOCK_PROTECT
#ifndef DEADLOCK_PROTECT
#define UI_TREE_LOCK  do {	 \
	TD_GET(splay_lock) = 0;	 \
	spinlock_lock(&ui_tree_lock);  \
	TD_GET(splay_lock) = 1;	 \
} while (0)

#define UI_TREE_UNLOCK  do {       \
	spinlock_unlock(&ui_tree_lock);  \
	TD_GET(splay_lock) = 0;	   \
} while (0)
#else // defined(DEADLOCK_PROTECT)

#ifdef USE_HW_THREAD_ID
#include <hardware-thread-id.h>
#define lock_val get_hw_tid()
#define safe_spinlock_lock hwt_limit_spinlock_lock
#else  // ! defined(USE_HW_THREAD_ID)
#define lock_val SPINLOCK_LOCKED_VALUE
#define safe_spinlock_lock limit_spinlock_lock
#endif // USE_HW_THREAD_ID
static size_t iter_count = 0;
extern void hpcrun_drop_sample(void);
static inline void
lock_ui(void)
{
  if (! safe_spinlock_lock(&ui_tree_lock, iter_count, lock_val)) {
	//    EMSG("Abandon Lock for hwt id = %d", lock_val);
	TD_GET(deadlock_drop) = true;
	hpcrun_stats_num_samples_yielded_inc();
	hpcrun_drop_sample();
  }
}

static inline void
unlock_ui(void) {
  spinlock_unlock(&ui_tree_lock);
}
#define UI_TREE_LOCK TD_GET(splay_lock) = 0; lock_ui(); TD_GET(splay_lock) = 1
#define UI_TREE_UNLOCK unlock_ui(); TD_GET(splay_lock) = 0
#endif // DEADLOCK_PROTECT

// Invariant: For any interval_ldmod_t key in the map, the corresponding
// bitree_uwi_t tree is not NULL:
static addr_to_recipe_map_t *addr2recipe_map = NULL;

// DXN: memory allocator for creating addr2recipe_map
// and inserting entries into addr2recipe_map:
static mem_alloc m_alloc = hpcrun_malloc;

static bitree_uwi_t *ui_free_list = NULL;
static size_t the_ui_size = 0;


extern btuwi_status_t build_intervals(char *ins, unsigned int len, mem_alloc m_alloc);
static void free_ui_tree_locked(bitree_uwi_t *tree);


//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------

#ifdef NONZERO_THRESHOLD
#define DEADLOCK_DEFAULT 5000
#else  // ! NONZERO_THRESHOLD ==> DEADLOCK_DEFAULT = 0
#define DEADLOCK_DEFAULT 0
#endif // NONZERO_THRESHOLD

void
uw_recipe_map_init(void)
{
  TMSG(UITREE, "init address-to-recipe map");
  addr2recipe_map = a2r_map_new(m_alloc);
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
uw_recipe_map_poisoned(uintptr_t start, uintptr_t end)
{
  ildmod_btuwi_pair_t* itpair =
	  ildmod_btuwi_pair_t_build(start, end, NULL,
		  bitree_uwi_new(NULL, NULL, NULL, m_alloc), m_alloc);
  return a2r_map_insert(addr2recipe_map, itpair, m_alloc);
}

bool
uw_recipe_map_lookup(void *addr,  load_module_t** ldmod, bitree_uwi_t **uwi)
{
  ildmod_btuwi_pair_t *ilm_btui = uw_recipe_map_lookup_ildmod_btuwi_pair(addr);
  if ( ilm_btui == NULL ) return false;
  load_module_t *lm = ildmod_btuwi_pair_loadmod(ilm_btui);
  if (lm == NULL) return false;
  if (ldmod) *ldmod = lm;
  bitree_uwi_t *ui = bitree_uwi_inrange(ildmod_btuwi_pair_btuwi(ilm_btui), (uintptr_t)addr);
  *uwi =  ui;
  return true;
}

/*
 *
 */
ildmod_btuwi_pair_t *
uw_recipe_map_lookup_ildmod_btuwi_pair(void *addr)
{
  load_module_t *lm;
  btuwi_status_t istat;

  /* See if addr is already in the map. */
  ildmod_btuwi_pair_t* ildmod_btuwi =
	  a2r_map_inrange_find(addr2recipe_map, (uintptr_t)addr);

  if (!ildmod_btuwi) {
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

	// enclosing addresses found; now pair it with an NULL bitree_uwi_t and
	// try to insert into map:
	ildmod_btuwi_pair_t* ibtuwi =
		ildmod_btuwi_pair_t_build((uintptr_t)fcn_start, (uintptr_t)fcn_end, lm,
			NULL, m_alloc);
	bool inserted = a2r_map_insert(addr2recipe_map, ibtuwi, m_alloc);
	if (inserted) {
	  ildmod_btuwi = ibtuwi;
	} else {
	  ildmod_btuwi = a2r_map_inrange_find(addr2recipe_map, (uintptr_t)addr);
	  goto found_in_map;
	}

	istat = build_intervals(fcn_start, fcn_end - fcn_start, m_alloc);

	if (istat.first == NULL) {
	  TMSG(UITREE, "BAD build_intervals failed: fcn range %p to %p",
		  fcn_start, fcn_end);
	  return (NULL);
	}
	ildmod_btuwi_pair_set_btuwi(ildmod_btuwi, bitree_uwi_rebalance(istat.first));
  }

  found_in_map: // invariant: ildmod_btuwi is non-null
  while(!ildmod_btuwi_pair_btuwi(ildmod_btuwi));
  TMSG(UITREE_LOOKUP, "found in unwind tree: addr %p", addr);
  return ildmod_btuwi;
}


/*
 * Remove intervals in the range [start, end) from the unwind interval
 * tree, and move the deleted nodes to the unwind free list.
 */
void
uw_recipe_map_delete_range(void* start, void* end)
{
#if 0
  bitree_uwi_t *del_tree;
  UI_TREE_LOCK;
  TMSG(UITREE, "begin unwind delete from %p to %p", start, end);
  if (ENABLED(UITREE_VERIFY)) {
	interval_tree_verify(addr2recipe_map, "UITREE");
  }
  interval_tree_delete(&addr2recipe_map, &del_tree, start, end);
  free_ui_tree_locked(del_tree);
  if (ENABLED(UITREE_VERIFY)) {
	interval_tree_verify(addr2recipe_map, "UITREE");
  }
  UI_TREE_UNLOCK;
#endif

  a2r_map_inrange_del_bulk_unsynch(addr2recipe_map, (uintptr_t)start, (uintptr_t)end, free_ui_node_locked );  // DXN: TODO
}


//---------------------------------------------------------------------
// private operations
//---------------------------------------------------------------------


/*
 * The ui free routines are in the private section in order to enforce
 * locking.  Free is only called from hpcrun_addr_to_interval() or
 * hpcrun_delete_ui_range() where the free list is already locked.
 *
 * Free the nodes in post order, since putting a node on the free list
 * modifies its child pointers.
 */
static void
free_ui_tree_locked(bitree_uwi_t *tree)
{
  // DXN: Let it leak!!!!
#if 0
  if (tree == NULL)
	return;

  free_ui_tree_locked(LEFT(tree));
  free_ui_tree_locked(RIGHT(tree));
  RIGHT(tree) = ui_free_list;
  ui_free_list = tree;
#endif
}


void
free_ui_node_locked(void *node)
{
  // DXN: Let it leak!!!!
#if 0
  RIGHT(node) = ui_free_list;
  ui_free_list = node;
#endif
}

#if 0
/*
 * Return a node from the free list if non-empty, else make a new one
 * via hpcrun_malloc().  Each architecture has a different struct
 * size, but they all extend interval tree nodes, which is all we use
 * here.
 *
 * Note: the claim is that all calls to hpcrun_ui_malloc() go through
 * hpcrun_addr_to_interval() and thus the free list doesn't need its
 * own lock (maybe a little sleazy, and be careful the code doesn't
 * grow to violate this assumption).
 */
void *
hpcrun_ui_malloc(size_t ui_size)
{
  void *ans;

  /* Verify that all calls have the same ui_size. */
  if (the_ui_size == 0)
	the_ui_size = ui_size;
  assert(ui_size == the_ui_size);

  if (ui_free_list != NULL) {
	ans = (void *)ui_free_list;
	ui_free_list = bitree_uwi_rightsubtree(ui_free_list);
  } else {
	ans = hpcrun_malloc(ui_size);
  }

  if (ans != NULL)
	memset(ans, 0, ui_size);

  return (ans);
}
#endif

//---------------------------------------------------------------------
// debug operations
//---------------------------------------------------------------------

void
uw_recipe_map_print(void)
{
  a2r_map_print(addr2recipe_map);
}

