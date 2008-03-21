/*
 * Splay-tree code for PC to unwind interval lookup and memo-ization.
 *
 * $Id$
 */

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include "find.h"
#include "intervals.h"
#include "spinlock.h"
#include "splay.h"
#include "pmsg.h"
#include "thread_data.h"


static interval_tree_node_t csprof_interval_tree_root = NULL;
static spinlock_t csprof_interval_tree_lock;
#if 0
static spinlock_t xed_spinlock = SPINLOCK_UNLOCKED;
#ifdef CSPROF_THREADS
#include <pthread.h>
#include <sys/types.h>

pthread_mutex_t xedlock = PTHREAD_MUTEX_INITIALIZER;

#endif

#endif

void
csprof_interval_tree_init(void)
{
  PMSG(SPLAY,"SPLAY:interval splay tree init");
  csprof_interval_tree_root = NULL;
  spinlock_unlock(&csprof_interval_tree_lock);
#if 0
#ifdef CSPROF_THREADS
  pthread_mutex_init(&xedlock,NULL);
#endif
#endif
}

void
csprof_release_splay_lock(void)
{
  spinlock_unlock(&csprof_interval_tree_lock);
}

#if 0
// utility to lock the xed library
static interval_status build_intervals(char *ins, unsigned int len){
  interval_status rv;

#ifdef CSPROF_THREADS
  pthread_mutex_lock(&xedlock);
#endif

  spinlock_lock(&xed_spinlock);
  PMSG(SPLAY,"SPLAY: calling l_build_intervals");
  rv = l_build_intervals(ins,len);
  spinlock_unlock(&xed_spinlock);

#ifdef CSPROF_THREADS
  pthread_mutex_unlock(&xedlock);
#endif
  return rv;
}
#endif

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
    struct interval_tree_node dummy;
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

#ifdef DEBUG_TARGET

#define TARGET_ADDR ((unsigned long) 0x400310)
int debugflag;
int debugonce = 1; 
#endif 

/*
 * Lookup the PC address in the interval tree and return a pointer to
 * the interval containing that address (the new root).  Grow the tree
 * lazily and memo-ize the answers.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 */
#define root  csprof_interval_tree_root
#define lock  csprof_interval_tree_lock

#define SPINLOCK(l) do { \
    TD_GET(splay_lock) = 0; \
    spinlock_lock(l);       \
    TD_GET(splay_lock) = 1; } while(0);

#define SPINUNLOCK(l) do {\
    spinlock_unlock(l);       \
    TD_GET(splay_lock) = 0; } while(0);


unwind_interval *
csprof_addr_to_interval(void *addr)
{
    void *fcn_start, *fcn_end;
    interval_status istat;
    interval_tree_node_t first, last, p, lroot;
    unwind_interval *ans;
    int ret;

    SPINLOCK(&lock);

    /* See if addr is already in the tree. */
    root = interval_tree_splay(root, addr);
    if (root != NULL && START(root) <= addr && addr < END(root)) {
	lroot = root;
	SPINUNLOCK(&lock);
	PMSG(SPLAY,"SPLAY:found %lx already in tree",addr);
	return (unwind_interval *)lroot;
    }

#ifdef DEBUG_TARGET
    if (addr == TARGET_ADDR) debugflag = debugonce;
#endif

    /* Get list of new intervals to insert into the tree. */
    ret = find_enclosing_function_bounds((char *)addr, &fcn_start, &fcn_end);
    if (ret != SUCCESS) {
	SPINUNLOCK(&lock);
	PMSG(SPLAY,"SPLAY: no enclosing bounds found");
	return (NULL);
    }
    assert(fcn_start <= addr && addr < fcn_end);

    istat = build_intervals(fcn_start, fcn_end - fcn_start);

    if (istat.first == NULL) {
	SPINUNLOCK(&lock);
	PMSG(SPLAY,"SPLAY: build intervals failed");
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
	    ans = (unwind_interval *)p;
	last = p;
    }
    assert(fcn_start <= START(first));
    assert(END(last) <= fcn_end);

#ifdef DEBUG_TARGET
    if (debugflag) {
       unwind_interval *u;
       for(u = istat.first; u; u = NEXT(u)) {
         idump(u);
       }
    }
#endif

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

#ifdef DEBUG_TARGET
    if (addr == TARGET_ADDR) {
	debugonce = 0;
	debugflag = debugonce;
    }
#endif

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
