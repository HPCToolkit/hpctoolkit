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
// Copyright ((c)) 2002-2021, Rice University
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
 *  Splay Tree Macros for regular and interval trees.
 *
 *  Copyright (c) 2002-2021, Rice University.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id$
 */

#ifndef  _SPLAY_TREE_MACROS_
#define  _SPLAY_TREE_MACROS_

#include <stdlib.h>

/*
 *  The Sleator-Tarjan top-down splay algorithm for regular,
 *  single-key trees.
 *
 *  This macro is the body of the splay function.  It rotates the node
 *  containing "key" to the root, if there is one, else the new root
 *  will be an adjacent node (left or right).
 *
 *  The general macro takes 2 comparisons as arguments
 *   [ Frequently, only 1 is necessary, but occasionally, when the keys are
 *   not a primitive data type, the lt and gt operations may not show the
 *   same symmetry as the purely mathematical operations.
 *
 *     lt(a, b)  // defines the "less than" comparison
 *     gt(a, b)  // defines the "greater than" comparison
 *
 *  Nodes in the tree should be a struct with name "type" containing
 *  at least these field names with these types:
 *
 *    lt_field: the field of the key used with "less than" comparisons
 *    gt_field: the field of the key used with "greater than" comparisons
 *
 *    left    : struct type *,
 *    right   : struct type *.
 *
 *   NB: lt_field and gt_field are frequently the same field, but, in general,
 *       they can be different
 *
 *  "root" is a struct type * and is reset to the new root.
 *
 */

#define GENERAL_SPLAY_TREE(type, root, key, lt_field, gt_field, left, right, lt, gt) \
    struct type dummy_node;                                                          \
    struct type *ltree_max, *rtree_min, *yy;                                         \
    if ((root) != NULL) {                                                            \
	ltree_max = rtree_min = &dummy_node;                                         \
	for (;;) {                                                                   \
            if (lt((key), (root)->lt_field)) {                                       \
		if ((yy = (root)->left) == NULL){                                     \
		    break;  }                                                         \
		if (lt((key), yy->lt_field)) {                                       \
		    (root)->left = yy->right;                                        \
		    yy->right = (root);                                              \
		    (root) = yy;                                                     \
		    if ((yy = (root)->left) == NULL){                                 \
			break;  }                                                     \
		}                                                                    \
		rtree_min->left = (root);                                            \
		rtree_min = (root); 												\
	    } else if (gt((key), (root)->gt_field)) {                                \
		if ((yy = (root)->right) == NULL) {                                   \
		    break; }                                                          \
		if (gt((key), yy->gt_field)) {                                       \
		    (root)->right = yy->left;                                        \
		    yy->left = (root);                                               \
		    (root) = yy;                                                     \
		    if ((yy = (root)->right) == NULL){                                \
			break; }                                                      \
		}                                                                    \
		ltree_max->right = (root);                                           \
		ltree_max = (root);  												\
	    } else                                                                   \
		break;                                                               \
	    (root) = yy;                                                             \
	}                                                                            \
	ltree_max->right = (root)->left;                                             \
	rtree_min->left = (root)->right;                                             \
	(root)->left = dummy_node.right;                                             \
	(root)->right = dummy_node.left;                                             \
    }


/*
 *  The Sleator-Tarjan top-down splay algorithm for regular,
 *  single-key trees. This kind of splay tree uses the
 *  builtin < and > as comparison operations, and the lt_field
 *  and gt_field are the same (called 'value' in the derived macro)
 *
 */

#define lcl_builtin_lt(a, b) ((a) < (b))
#define lcl_builtin_gt(a, b) ((a) > (b))

#define REGULAR_SPLAY_TREE(type, root, key, value, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, value, value, left, right, lcl_builtin_lt, lcl_builtin_gt)

/*
 *  The Sleator-Tarjan top-down splay algorithm for interval trees.
 *
 *  This macro is the body of the splay function.  It rotates the
 *  interval containing "key" to the root, if there is one, else the
 *  new root will be an adjacent interval (left or right).
 *
 *  Nodes in the tree should be a struct with name "type" containing
 *  at least these four field names with these types:
 *
 *    start : same type as key,
 *    end   : same type as key,
 *    left  : struct type *,
 *    right : struct type *.
 *
 *  "root" is a struct type * and is reset to the new root.
 *
 *  Intervals are semi-inclusive: [start, end).
 */

#define lcl_intvl_lt(a, b) ((a) < (b))
#define lcl_intvl_gt(a, b) ((a) >= (b))

#define INTERVAL_SPLAY_TREE(type, root, key, start, end, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, start, end, left, right, lcl_intvl_lt, lcl_intvl_gt)

#endif  /* ! _SPLAY_TREE_MACROS_ */
