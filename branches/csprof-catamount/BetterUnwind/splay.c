/*
 * Splay-tree code for PC to unwind interval lookup and memo-ization.
 *
 * $Id$
 */

#include <err.h>
#include <stdio.h>
#include "find.h"
#include "intervals.h"
#include "simple-lock.h"
#include "splay.h"
#include "general.h"
#include "pmsg.h"

#ifdef CSPROF_THREADS
#include <pthread.h>
#include <sys/types.h>

pthread_mutex_t xedlock = PTHREAD_MUTEX_INITIALIZER;

#endif


static interval_tree_node_t csprof_interval_tree_root = NULL;
static simple_spinlock csprof_interval_tree_lock;

void
csprof_interval_tree_init(void)
{
    csprof_interval_tree_root = NULL;
    simple_spinlock_unlock(&csprof_interval_tree_lock);
#ifdef CSPROF_THREADS
    pthread_mutex_init(&xedlock,NULL);
#endif
}

// utility to lock the xed library
static interval_status build_intervals(char *ins, unsigned int len){
  interval_status rv;

#ifdef CSPROF_THREADS
  pthread_mutex_lock(&xedlock);
#endif

  MSG(1,"SPLAY: calling l_build_intervals");
  rv = l_build_intervals(ins,len);

#ifdef CSPROF_THREADS
  pthread_mutex_unlock(&xedlock);
#endif
  return rv;
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
interval_tree_splay(interval_tree_node_t root, unsigned long addr)
{
    struct interval_tree_node dummy;
    interval_tree_node_t ltree_max, rtree_min, y;

    if (root == NULL)
	return (NULL);

    ltree_max = rtree_min = &dummy;
    for (;;) {
	/* root is never NULL in here. */
	if (addr < root->start) {
	    if ((y = root->left) == NULL)
		break;
	    if (addr < y->start) {
		/* rotate right */
		root->left = y->right;
		y->right = root;
		root = y;
		if ((y = root->left) == NULL)
		    break;
	    }
	    /* Link new root into right tree. */
	    rtree_min->left = root;
	    rtree_min = root;
	} else if (addr >= root->end) {
	    if ((y = root->right) == NULL)
		break;
	    if (addr >= y->end) {
		/* rotate left */
		root->right = y->left;
		y->left = root;
		root = y;
		if ((y = root->right) == NULL)
		    break;
	    }
	    /* Link new root into left tree. */
	    ltree_max->right = root;
	    ltree_max = root;
	} else
	    break;
	root = y;
    }

    /* Assemble the new root. */
    ltree_max->right = root->left;
    rtree_min->left = root->right;
    root->left = dummy.right;
    root->right = dummy.left;

    return (root);
}

/*
 * Lookup the PC address in the interval tree and return a pointer to
 * the interval containing that address (the new root).  Grow the tree
 * lazily and memo-ize the answers.
 *
 * Returns: pointer to unwind_interval struct if found, else NULL.
 */
#define root  csprof_interval_tree_root
#define lock  csprof_interval_tree_lock
unwind_interval *
csprof_addr_to_interval(unsigned long addr)
{
    char *fcn_start, *fcn_end;
    interval_status istat;
    interval_tree_node_t first, last, p;
    unwind_interval *ans;
    int ret;

    simple_spinlock_lock(&lock);

    /* See if addr is already in the tree. */
    root = interval_tree_splay(root, addr);
    if (root != NULL && root->start <= addr && addr < root->end) {
	simple_spinlock_unlock(&lock);
	MSG(1,"SPLAY:found %lx already in tree",addr);
	return (unwind_interval *)root;
    }

    /* Get list of new intervals to insert into the tree. */
    ret = find_enclosing_function_bounds((char *)addr, &fcn_start, &fcn_end);
    if (ret != SUCCESS) {
	simple_spinlock_unlock(&lock);
	MSG(1,"SPLAY: no enclosing bounds found");
	return (NULL);
    }
#ifdef OLD_INTERFACE
    istat = l_build_intervals(fcn_start, fcn_end - fcn_start);
#else
    istat = build_intervals(fcn_start, fcn_end - fcn_start);
#endif //OLD_INTERFACE
    if (istat.first == NULL) {
	simple_spinlock_unlock(&lock);
	MSG(1,"SPLAY: build intervals failed");
	return (NULL);
    }

    /*
     * Convert list of unwind intervals to tree nodes and look for the
     * interval containing addr.
     * FIXME: Should do more sanity-checking on the order of intervals.
     */
    first = (interval_tree_node_t)istat.first;
    ans = NULL;
    for (p = first; p != NULL; p = p->right) {
	p->left = NULL;
	if (p->start <= addr && addr < p->end)
	    ans = (unwind_interval *)p;
	last = p;
    }

    /*
     * Always link the new nodes into the tree whether we found the
     * interval or not.
     * FIXME: Should do more sanity-checking on the order of intervals.
     */
    if (root == NULL) {
	root = first;
    } else if (addr < root->start) {
	if (root->left == NULL) {
	    if (last->end <= root->start) {
		root->left = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	} else {
	    root->left = interval_tree_splay(root->left, root->end);
	    if (root->left->end <= first->start && last->end <= root->start) {
		root->left->right = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	}
    } else {
	if (root->right == NULL) {
	    if (root->end <= first->start) {
		root->right = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	} else {
	    root->right = interval_tree_splay(root->right, root->start);
	    if (root->end <= first->start && last->end <= root->right->start) {
		root->right->left = first;
	    } else {
		EMSG("%s: bad unwind interval at 0x%lx", __func__, addr);
	    }
	}
    }

    simple_spinlock_unlock(&lock);
    MSG(1,"SPLAY: returning interval = %p",ans);
    return (ans);
}
