/*
 * Splay-tree code for PC to unwind interval lookup and memo-ization.
 *
 * $Id$
 */

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include "splay-interval.h"
#include "spinlock.h"
#include "splay.h"
#include "pmsg.h"
#include "thread_data.h"

#include "fnbounds_interface.h"


static interval_tree_node_t csprof_interval_tree_root = NULL;
static spinlock_t csprof_interval_tree_lock;

void
csprof_interval_tree_init(void)
{
  TMSG(SPLAY,"interval splay tree init");
  csprof_interval_tree_root = NULL;
  spinlock_unlock(&csprof_interval_tree_lock);
}

void
csprof_release_splay_lock(void)
{
  spinlock_unlock(&csprof_interval_tree_lock);
}

/*
 * The Sleator-Tarjan top-down splay algorithm.  Rotate the interval
 * containing "addr" to the root, if there is one, else move an
 * adjacent interval (left or right) to the root.  Note: intervals are
 * semi-inclusive: [start, end).
 *
 * Returns: the new root.
 */
static interval_tree_node_t
interval_tree_splay(interval_tree_node_t root, void *addr)
{
    interval_tree_node dummy;
    interval_tree_node_t ltree_max, rtree_min, y;

    if (root == NULL)
	return (NULL);

    ltree_max = rtree_min = &dummy;
    for (;;) {
	/* root is never NULL in here. */
	if (addr < START(root)) {
	    if ((y = LEFT(root)) == NULL)
		break;
	    if (addr < START(y)) {
		/* rotate right */
		LEFT(root) = RIGHT(y);
		RIGHT(y) = root;
		root = y;
		if ((y = LEFT(root)) == NULL)
		    break;
	    }
	    /* Link new root into right tree. */
	    LEFT(rtree_min) = root;
	    rtree_min = root;
	} else if (addr >= END(root)) {
	    if ((y = RIGHT(root)) == NULL)
		break;
	    if (addr >= END(y)) {
		/* rotate left */
		RIGHT(root) = LEFT(y);
		LEFT(y) = root;
		root = y;
		if ((y = RIGHT(root)) == NULL)
		    break;
	    }
	    /* Link new root into left tree. */
	    RIGHT(ltree_max) = root;
	    ltree_max = root;
	} else
	    break;
	root = y;
    }

    /* Assemble the new root. */
    RIGHT(ltree_max) = LEFT(root);
    LEFT(rtree_min) = RIGHT(root);
    LEFT(root) = SRIGHT(dummy);
    RIGHT(root) = SLEFT(dummy);

    return (root);
}

/*
 * Lookup the PC address in the interval tree and return a pointer to
 * the interval containing that address (the new root).  Grow the tree
 * lazily and memo-ize the answers.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 */
// FIXME  --- this is dangerous and sucks
#define root  csprof_interval_tree_root
#define lock  csprof_interval_tree_lock

#define SPINLOCK(l) do { \
    TD_GET(splay_lock) = 0; \
    spinlock_lock(l);       \
    TD_GET(splay_lock) = 1; } while(0);

#define SPINUNLOCK(l) do {\
    spinlock_unlock(l);       \
    TD_GET(splay_lock) = 0; } while(0);


splay_interval_t *
csprof_addr_to_interval(void *addr)
{
    void *fcn_start, *fcn_end;
    interval_status istat;
    interval_tree_node_t first, last, p, lroot;
    splay_interval_t *ans;
    int ret;
    extern interval_status build_intervals(char *ins, unsigned int len);

    SPINLOCK(&lock);

    /* See if addr is already in the tree. */
    root = interval_tree_splay(root, addr);
    if (root != NULL && START(root) <= addr && addr < END(root)) {
	lroot = root;
	SPINUNLOCK(&lock);
	TMSG(SPLAY,"found %lx already in tree",addr);
	return (splay_interval_t *)lroot;
    }

    /* Get list of new intervals to insert into the tree. */
    ret = fnbounds_enclosing_addr(addr, &fcn_start, &fcn_end);
    if (ret != SUCCESS) {
	SPINUNLOCK(&lock);
	TMSG(SPLAY,"no enclosing bounds found");
	return (NULL);
    }
    assert(fcn_start <= addr && addr < fcn_end);

    istat = build_intervals(fcn_start, fcn_end - fcn_start);

    if (istat.first == NULL) {
	SPINUNLOCK(&lock);
	TMSG(SPLAY,"build intervals failed");
	return (NULL);
    }

    /*
     * Convert list of unwind intervals to tree nodes and look for the
     * interval containing addr.
     * FIXME: Should do more sanity-checking on the order of intervals.
     */
    first = (interval_tree_node_t)istat.first;
    ans = NULL;
    for (p = first; p != NULL; p = RIGHT(p)) {
	LEFT(p) = NULL;
	if (START(p) <= addr && addr < END(p))
	    ans = p;
	last = p;
    }
    assert(fcn_start <= START(first));
    assert(END(last) <= fcn_end);

    /*
     * Always link the new nodes into the tree whether we found the
     * interval or not.
     * FIXME: Should do more sanity-checking on the order of intervals.
     */
    if (root == NULL) {
	root = first;
    } else if (addr < START(root)) {
	if (LEFT(root) == NULL) {
	    if (END(last) <= START(root)) {
		LEFT(root) = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	} else {
	    LEFT(root) = interval_tree_splay(LEFT(root), END(root));
	    if (END(LEFT(root)) <= START(first) && END(last) <= START(root)) {
		RIGHT(LEFT(root)) = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	}
    } else {
	if (RIGHT(root) == NULL) {
	    if (END(root) <= START(first)) {
		RIGHT(root) = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	} else {
	    RIGHT(root) = interval_tree_splay(RIGHT(root), START(root));
	    if (END(root) <= START(first) && END(last) <= START(RIGHT(root))) {
		LEFT(RIGHT(root)) = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	}
    }

    SPINUNLOCK(&lock);

    PMSG(SPLAY,"SPLAY: returning interval = %p",ans);
    return (ans);
}

#undef root

/* *********************************************************************** */

static void
csprof_print_interval_tree_r(FILE* fs, interval_tree_node_t node);


void
csprof_print_interval_tree()
{
  FILE* fs = stdout;

  fprintf(fs, "Interval tree:\n");
  csprof_print_interval_tree_r(fs, csprof_interval_tree_root);
  fprintf(fs, "\n");
  fflush(fs);
}


static void
csprof_print_interval_tree_r(FILE* fs, interval_tree_node_t node)
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
