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
 * General interval splay tree functions, not specific to unwind
 * intervals.
 *
 * Note: these routines all assume a locked tree.
 *
 * $Id$
 */

#include <stdio.h>

#include "splay.h"
#include "splay-interval.h"

#include <messages/messages.h>

//---------------------------------------------------------------------
// private operations
//---------------------------------------------------------------------


/*
 * The Sleator-Tarjan top-down splay algorithm.  Rotate the interval
 * containing "addr" to the root, if there is one, else move an
 * adjacent interval (left or right) to the root.  Note: intervals are
 * semi-inclusive: [start, end).
 *
 * Returns: the new root.
 */
static interval_tree_node *
interval_tree_splay(interval_tree_node *root, void *addr)
{
    interval_tree_node dummy;
    interval_tree_node *ltree_max, *rtree_min, *y;

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


//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------


/*
 * Lookup "addr" in the tree "root".
 *
 * Returns: pointer to node containing "addr", else NULL if "addr" is
 * not in the tree.
 */
interval_tree_node *
interval_tree_lookup(interval_tree_node **root,  /* in/out */
		     void *addr)
{
    *root = interval_tree_splay(*root, addr);
    if (*root != NULL && START(*root) <= addr && addr < END(*root))
	return (*root);

    return (NULL);
}


/*
 * Insert "node" into the tree at "root", and check for nodes that are
 * reversed (start > end), zero-length (start = end), or that overlap
 * with an existing node.
 *
 * Returns: 0 on success (inserted), else 1 if insert failed
 * (overlap).
 */
int
interval_tree_insert(interval_tree_node **root,  /* in/out */
		     interval_tree_node *node)
{
    interval_tree_node *t;

    /* Reversed order or zero-length. */
    if (START(node) >= END(node))
	return (1);

    /* Empty tree. */
    if (*root == NULL) {
	LEFT(node) = NULL;
	RIGHT(node) = NULL;
	*root = node;
	return (0);
    }

    /* Insert left of root. */
    *root = interval_tree_splay(*root, START(node));
    if (END(node) <= START(*root)) {
	LEFT(node) = LEFT(*root);
	RIGHT(node) = *root;
	LEFT(*root) = NULL;
	*root = node;
	return (0);
    }

    /* Insert right of root. */
    t = interval_tree_splay(RIGHT(*root), START(*root));
    if (END(*root) <= START(node)
	&& (t == NULL || END(node) <= START(t))) {
	LEFT(node) = *root;
	RIGHT(node) = t;
	RIGHT(*root) = NULL;
	*root = node;
	return (0);
    }

    /* Must overlap with something in the tree. */
    RIGHT(*root) = t;
    return (1);
}


/*
 * Remove from "root" all nodes with intervals within or overlapping
 * with [start, end).
 *
 * Returns: the new root in *root and the tree of deleted nodes in
 * *del_tree.
 */
void
interval_tree_delete(interval_tree_node **root,      /* in/out */
		     interval_tree_node **del_tree,  /* out */
		     void *start, void *end)
{
    interval_tree_node *ltree, *rtree, *t;

    /* Empty tree. */
    if (*root == NULL) {
	*del_tree = NULL;
	return;
    }

    /*
     * Split the tree into three pieces: intervals entirely less than
     * start (ltree), intervals within or overlapping with [start, end)
     * (del_tree), and intervals entirely greater than end (rtree).
     */
    t = interval_tree_splay(*root, start);
    if (END(t) <= start) {
	ltree = t;
	t = RIGHT(t);
	RIGHT(ltree) = NULL;
    } else {
	ltree = LEFT(t);
	LEFT(t) = NULL;
    }
    t = interval_tree_splay(t, end);
    if (t == NULL) {
	*del_tree = NULL;
	rtree = NULL;
    } else if (end <= START(t)) {
	*del_tree = LEFT(t);
	rtree = t;
	LEFT(t) = NULL;
    } else {
	*del_tree = t;
	rtree = RIGHT(t);
	RIGHT(t) = NULL;
    }

    /* Combine the left and right pieces to make the new tree. */
    if (ltree == NULL) {
	*root = rtree;
    } else if (rtree == NULL) {
	*root = ltree;
    } else {
	*root = interval_tree_splay(ltree, end);
	RIGHT(*root) = rtree;
    }
}


/*
 * Verify the interval tree "root" by doing an in-order traversal and
 * checking each node for internal consistency (start < end) and for
 * overlap with the previous node.
 */
static int v_num_errs;
static int v_prev_num;
static void *v_prev_start;
static void *v_prev_end;
static void *v_min_addr;
static unsigned long v_total_len;

static void
verify_tree(interval_tree_node *root, const char *label)
{
    if (root == NULL)
	return;

    verify_tree(LEFT(root), label);

    /* reversed order */
    if (START(root) >= END(root)) {
	EMSG("%s: BAD node %d [%p, %p) in reversed order",
	     label, v_prev_num + 1, START(root), END(root));
	v_num_errs++;
    }
    /* overlap */
    if (v_prev_end > START(root)) {
	EMSG("%s: BAD node %d [%p, %p) overlaps node %d [%p, %p)",
	     label, v_prev_num, v_prev_start, v_prev_end,
	     v_prev_num + 1, START(root), END(root));
	v_num_errs++;
    }
    v_prev_num++;
    v_prev_start = START(root);
    v_prev_end = END(root);
    if (v_prev_num == 1) {
	v_min_addr = START(root);
    }
    v_total_len += (unsigned long)END(root) - (unsigned long)START(root);

    verify_tree(RIGHT(root), label);
}


void
interval_tree_verify(interval_tree_node *root, const char *label)
{
    v_num_errs = 0;
    v_prev_num = 0;
    v_prev_start = (void *)0;
    v_prev_end = (void *)0;
    v_min_addr = (void *)0;
    v_total_len = 0;

    verify_tree(root, label);
    EMSG("%s: verify: errors = %d, nodes = %d, min = %p, max = %p, len = %lu",
	 label, v_num_errs, v_prev_num, v_min_addr, v_prev_end, v_total_len);
}
