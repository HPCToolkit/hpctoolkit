// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include "csprof-malloc.h"
#include "fnbounds_interface.h"
#include "splay.h"
#include "splay-interval.h"
#include "thread_data.h"
#include "ui_tree.h"

#include <messages/messages.h>

#include <lib/prof-lean/spinlock.h>


#define UI_TREE_LOCK  do {	 \
  TD_GET(splay_lock) = 0;	 \
  spinlock_lock(&ui_tree_lock);  \
  TD_GET(splay_lock) = 1;	 \
} while (0)

#define UI_TREE_UNLOCK  do {       \
  spinlock_unlock(&ui_tree_lock);  \
  TD_GET(splay_lock) = 0;	   \
} while (0)


// Locks both the UI tree and the UI free list.
static spinlock_t ui_tree_lock;

static interval_tree_node *ui_tree_root = NULL;
static interval_tree_node *ui_free_list = NULL;
static size_t the_ui_size = 0;


extern interval_status build_intervals(char *ins, unsigned int len);
static void free_ui_tree_locked(interval_tree_node *tree);


//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------


void
csprof_interval_tree_init(void)
{
  TMSG(UITREE, "init unwind interval tree");
  ui_tree_root = NULL;
  UI_TREE_UNLOCK;
}


void
csprof_release_splay_lock(void)
{
  UI_TREE_UNLOCK;
}


/*
 * Return a node from the free list if non-empty, else make a new one
 * via csprof_malloc().  Each architecture has a different struct
 * size, but they all extend interval tree nodes, which is all we use
 * here.
 *
 * Note: the claim is that all calls to csprof_ui_malloc() go through
 * csprof_addr_to_interval() and thus the free list doesn't need its
 * own lock (maybe a little sleazy, and be careful the code doesn't
 * grow to violate this assumption).
 */
void *
csprof_ui_malloc(size_t ui_size)
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
    ans = csprof_malloc(ui_size);
  }

  if (ans != NULL)
    memset(ans, 0, ui_size);

  return (ans);
}


/*
 * Lookup the PC address in the interval tree and return a pointer to
 * the interval containing that address (the new root).  Grow the tree
 * lazily and memo-ize the answers.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 */
splay_interval_t *
csprof_addr_to_interval(void *addr)
{
  UI_TREE_LOCK;
  splay_interval_t *retval = csprof_addr_to_interval_locked(addr);
  UI_TREE_UNLOCK;

  return retval;
}


/*
 * The locked version of csprof_addr_to_interval().  Lookup the PC
 * address in the interval tree and return a pointer to the interval
 * containing that address.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 * The caller must hold the ui-tree lock.
 */
splay_interval_t *
csprof_addr_to_interval_locked(void *addr)
{
  void *fcn_start, *fcn_end;
  interval_status istat;
  interval_tree_node *p, *q;
  splay_interval_t *ans;
  int ret;

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
  ret = fnbounds_enclosing_addr(addr, &fcn_start, &fcn_end);
  UI_TREE_LOCK;
  if (ret != SUCCESS) {
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
csprof_delete_ui_range(void *start, void *end)
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
 * locking.  Free is only called from csprof_addr_to_interval() or
 * csprof_delete_ui_range() where the free list is already locked.
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


static void
csprof_print_interval_tree_r(FILE* fs, interval_tree_node *node);


void
csprof_print_interval_tree()
{
  FILE* fs = stdout;

  fprintf(fs, "Interval tree:\n");
  csprof_print_interval_tree_r(fs, ui_tree_root);
  fprintf(fs, "\n");
  fflush(fs);
}


static void
csprof_print_interval_tree_r(FILE* fs, interval_tree_node *node)
{
  // Base case
  if (node == NULL) {
    return;
  }

  fprintf(fs, "  {%p start=%p end=%p}\n", node, START(node), END(node));

  // Recur
  csprof_print_interval_tree_r(fs, RIGHT(node));
  csprof_print_interval_tree_r(fs, LEFT(node));
}
