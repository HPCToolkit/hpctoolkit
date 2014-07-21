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
// Copyright ((c)) 2002-2014, Rice University
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
#include "splay.h"
#include "splay-interval.h"
#include "thread_data.h"
#include "ui_tree.h"

#include <messages/messages.h>

// libmonitor functions
#include <monitor.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

// !!! FIXME: MWF debug deadlock problem !!!

pthread_t ui_lock_holder = -1;
bool lock_trace = false;
#define UI_LOCK_TRACE_LEN 1000
int ui_lock_trace[UI_LOCK_TRACE_LEN] = {};
size_t ui_trace_cursor = 0;

// Locks both the UI tree and the UI free list.
spinlock_t ui_tree_lock = SPINLOCK_UNLOCKED;

#define DEBUG_LOCK
#ifndef DEBUG_LOCK
#define UI_TREE_LOCK  do {	 \
  TD_GET(splay_lock) = 0;	 \
  spinlock_lock(&ui_tree_lock);  \
  ui_lock_holder = monitor_get_thread_num(); \
  TD_GET(splay_lock) = 1;	 \
} while (0)

#define UI_TREE_UNLOCK  do {       \
  ui_lock_holder = -1;             \
  spinlock_unlock(&ui_tree_lock);  \
  TD_GET(splay_lock) = 0;	   \
} while (0)
#else // The debug variety
//
// Deadlock protection
//  -- !!! clean up later !!! --
//
#define WRITE_MSG 1
static size_t iter_count = 0;
extern void hpcrun_drop_sample(void);
static inline void
lock_ui(void)
{
  char buf[100] = {};
  for(size_t i=0;;i++) {
    if (iter_count && (i > iter_count)) {
#ifdef FPRINTF_MSG
      fprintf(stderr, "Thread %d exceeded iter_count\n", monitor_get_thread_num());
#elif defined(WRITE_MSG)
      int len = snprintf(buf, sizeof(buf), "Thread %d exceeded iter_count\n", monitor_get_thread_num());
      write(2, buf, len > 0 ? len : sizeof(buf) - 1);
#endif // FPRINTF_MSG
      hpcrun_drop_sample();
    }
    while(ui_tree_lock.thelock != SPINLOCK_UNLOCKED_VALUE) {
      if (iter_count && (i > iter_count)) {
#ifdef FPRINTF_MSG
	fprintf(stderr, "Thread %d exceeded iter_count\n", monitor_get_thread_num());
#elif defined(WRITE_MSG)
      int len = snprintf(buf, sizeof(buf), "Thread %d exceeded iter_count\n", monitor_get_thread_num());
      write(2, buf, len > 0 ? len : sizeof(buf) - 1);
#endif // FPRINTF_MSG
	hpcrun_drop_sample();
      }
      i++;
    }
    if (fetch_and_store(&(ui_tree_lock.thelock),
			SPINLOCK_LOCKED_VALUE) == SPINLOCK_UNLOCKED_VALUE)
      break;
  }
  if (lock_trace)
    ui_lock_trace[ui_trace_cursor] = monitor_get_thread_num();
}
static inline void
unlock_ui(void) {
  if (lock_trace && (ui_trace_cursor < UI_LOCK_TRACE_LEN))
    fetch_and_add(&ui_trace_cursor, 1);
  ui_tree_lock.thelock = SPINLOCK_UNLOCKED_VALUE;
}
#define UI_TREE_LOCK lock_ui(); ui_lock_holder = monitor_get_thread_num()
#define UI_TREE_UNLOCK ui_lock_holder = -1; unlock_ui()
#endif //

// Locks both the UI tree and the UI free list.
// static spinlock_t ui_tree_lock;

static interval_tree_node *ui_tree_root = NULL;
static interval_tree_node *ui_free_list = NULL;
static size_t the_ui_size = 0;


extern interval_status build_intervals(char *ins, unsigned int len);
static void free_ui_tree_locked(interval_tree_node *tree);


//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------

void
hpcrun_interval_tree_init(void)
{
  TMSG(UITREE, "init unwind interval tree");
  ui_tree_root = NULL;
  for (size_t i=0; i < UI_LOCK_TRACE_LEN; i++)
    ui_lock_trace[i] = -1;
  if (getenv("LOCK_TRACE")) lock_trace = true;
  iter_count = 0;
  if (getenv("DEADLOCK_THRESH"))
    iter_count = atoi(getenv("DEADLOCK_THRESH"));
  UI_TREE_UNLOCK;
}


void
hpcrun_release_splay_lock(void)
{
  UI_TREE_UNLOCK;
}


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
    ui_free_list = RIGHT(ui_free_list);
  } else {
    ans = hpcrun_malloc(ui_size);
  }

  if (ans != NULL)
    memset(ans, 0, ui_size);

  return (ans);
}


/*
 * Lookup the instruction pointer 'addr' in the interval tree and
 * return a pointer to the interval containing that address (the new
 * root).  Grow the tree lazily and memo-ize the answers.
 *
 * Extension to support fast normalization of IPs: Given an (dynamic)
 * IP 'ip', return the normalized (static) IP through the argument
 * 'ip_norm'.  Both 'ip' and 'ip_norm' are optional and may be NULL.
 * NOTE: typically 'addr' equals 'ip', but there are subtle use cases
 * where they are different.  (In all cases, 'addr' and 'ip' should be
 * in the same load module.)
 *
 * N.B.: tallent: The above extension is not as clean as I would like.
 * An alternative is to write a separate function, but then the
 * typical usage would require two back-to-back lock-protected
 * lookups.  Thus for the time being, in this limited circumstance, I
 * have chosen performance over a cleaner design.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 */

bool ui_lock_holder_ok(void);

splay_interval_t *
hpcrun_addr_to_interval(void *addr, void *ip, ip_normalized_t* ip_norm)
{
  assert(ui_lock_holder_ok());
#ifdef NO
  if (! TD_GET(btbuf_cur)) {
    EMSG("Before ui lock btbuf_cur = %p", TD_GET(btbuf_cur));
    // monitor_real_abort();
  }
#endif
  UI_TREE_LOCK;
  splay_interval_t *intvl = hpcrun_addr_to_interval_locked(addr);
  UI_TREE_UNLOCK;
  
  if (ip && ip_norm && intvl) {
    *ip_norm = hpcrun_normalize_ip(ip, intvl->lm);
  }
  
  return intvl;
}


/*
 * The locked version of hpcrun_addr_to_interval().  Lookup the PC
 * address in the interval tree and return a pointer to the interval
 * containing that address.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 * The caller must hold the ui-tree lock.
 */
splay_interval_t *
hpcrun_addr_to_interval_locked(void *addr)
{
  void *fcn_start, *fcn_end;
  load_module_t *lm;
  interval_status istat;
  interval_tree_node *p, *q;
  splay_interval_t *ans;

  /* See if addr is already in the tree. */
  p = interval_tree_lookup(&ui_tree_root, addr);
  if (p != NULL) {
    TMSG(UITREE_LOOKUP, "found in unwind tree: addr %p", addr);
    return (splay_interval_t *)p;
  }

  /*
   * Get list of new intervals to insert into the tree.
   *
   * Note: we can't hold the ui-tree lock while acquiring the fnbounds
   * lock, that could lead to LOR deadlock.  Releasing and reacquiring
   * the lock could cause the insert to fail if another thread does
   * the insert first, but in that case, it's not really a failure.
   */
  UI_TREE_UNLOCK;
  bool ret = fnbounds_enclosing_addr(addr, &fcn_start, &fcn_end, &lm);
  UI_TREE_LOCK;
  if (! ret) {
    TMSG(UITREE, "BAD fnbounds_enclosing_addr failed: addr %p", addr);
    return (NULL);
  }
  if (addr < fcn_start || fcn_end <= addr) {
    TMSG(UITREE, "BAD fnbounds_enclosing_addr failed: addr %p "
	 "not within fcn range %p to %p", addr, fcn_start, fcn_end);
    return (NULL);
  }
  istat = build_intervals(fcn_start, fcn_end - fcn_start);
  if (istat.first == NULL) {
    TMSG(UITREE, "BAD build_intervals failed: fcn range %p to %p",
	 fcn_start, fcn_end);
    return (NULL);
  }

  TMSG(UITREE, "begin unwind insert addr %p, fcn range %p to %p",
       addr, fcn_start, fcn_end);
  if (ENABLED(UITREE_VERIFY)) {
    interval_tree_verify(ui_tree_root, "UITREE");
  }

  /* Insert the nodes and report on failures. */
  ans = NULL;
  for (p = istat.first; p != NULL; p = q)
  {
    q = RIGHT(p);
    if (START(p) >= END(p)) {
      TMSG(UITREE, "BAD unwind interval [%p, %p) reverse order",
	   START(p), END(p));
    }
    else if (START(p) < fcn_start || fcn_end < END(p)) {
      TMSG(UITREE, "BAD unwind interval [%p, %p) not within fcn range",
	   START(p), END(p));
      free_ui_node_locked(p);
    }
    else if (interval_tree_insert(&ui_tree_root, p) != 0) {
      TMSG(UITREE, "BAD unwind interval [%p, %p) insert failed",
	   START(p), END(p));
      free_ui_node_locked(p);
    }
    else {
      TMSG(UITREE, "unwind insert [%p, %p)", START(p), END(p));
    }
    if (START(p) <= addr && addr < END(p)) {
      ans = p;
    }
  }

  /* Memoize associated load module to benefit hpcrun_normalize_ip() */
  if (ans) {
    ans->lm = lm;
  }

  if (ENABLED(UITREE_VERIFY)) {
    interval_tree_verify(ui_tree_root, "UITREE");
  }

  TMSG(UITREE_LOOKUP, "unwind lookup, addr = %p, ans = %p", addr, ans);
  return (ans);
}


/*
 * Remove intervals in the range [start, end) from the unwind interval
 * tree, and move the deleted nodes to the unwind free list.
 */
void
hpcrun_delete_ui_range(void* start, void* end)
{
  interval_tree_node *del_tree;

  UI_TREE_LOCK;

  TMSG(UITREE, "begin unwind delete from %p to %p", start, end);
  if (ENABLED(UITREE_VERIFY)) {
    interval_tree_verify(ui_tree_root, "UITREE");
  }

  interval_tree_delete(&ui_tree_root, &del_tree, start, end);
  free_ui_tree_locked(del_tree);

  if (ENABLED(UITREE_VERIFY)) {
    interval_tree_verify(ui_tree_root, "UITREE");
  }

  UI_TREE_UNLOCK;
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
free_ui_tree_locked(interval_tree_node *tree)
{
  if (tree == NULL)
    return;

  free_ui_tree_locked(LEFT(tree));
  free_ui_tree_locked(RIGHT(tree));
  RIGHT(tree) = ui_free_list;
  ui_free_list = tree;
}


void
free_ui_node_locked(interval_tree_node *node)
{
  RIGHT(node) = ui_free_list;
  ui_free_list = node;
}


//---------------------------------------------------------------------
// debug operations
//---------------------------------------------------------------------

bool
ui_lock_holder_ok(void)
{
  bool rv = ui_lock_holder != monitor_get_thread_num();
  if (!rv) EMSG("Attempted to get UI_TREE_LOCK that I already own !!!");
  return rv;
}

static void
hpcrun_print_interval_tree_r(FILE* fs, interval_tree_node *node);


void
hpcrun_print_interval_tree()
{
  FILE* fs = stdout;

  fprintf(fs, "Interval tree:\n");
  hpcrun_print_interval_tree_r(fs, ui_tree_root);
  fprintf(fs, "\n");
  fflush(fs);
}


static void
hpcrun_print_interval_tree_r(FILE* fs, interval_tree_node *node)
{
  // Base case
  if (node == NULL) {
    return;
  }

  fprintf(fs, "  {%p start=%p end=%p}\n", node, START(node), END(node));

  // Recur
  hpcrun_print_interval_tree_r(fs, RIGHT(node));
  hpcrun_print_interval_tree_r(fs, LEFT(node));
}
