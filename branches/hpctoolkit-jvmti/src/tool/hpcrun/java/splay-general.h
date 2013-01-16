/*
 *  Splay Tree Macros (regular, non-interval).
 *
 *  $Id: splay-macros.h 2138 2010-12-16 21:10:14Z skw0897 $
 */

#ifndef  _REGULAR_SPLAY_TREE_
#define  _REGULAR_SPLAY_TREE_

#include <stdio.h>

/*
 *  The Sleator-Tarjan top-down splay algorithm.
 *
 *  This macro is the body of the splay function.  It rotates the node
 *  containing "key" to the root, if there is one, else the new root
 *  will be an adjacent node (left or right).
 *
 *  Nodes in the tree should be a struct with name "type" containing
 *  at least these three field names with these types:
 *
 *    value : same type as key,
 *    left  : struct type *,
 *    right : struct type *.
 *
 *  "root" is a struct type * and is reset to the new root.
 */

#define REGULAR_SPLAY_TREE(type, root, key, value, left, right)	\
    struct type dummy_node;					\
    struct type *ltree_max, *rtree_min, *y;			\
    if ((root) != NULL) {					\
	ltree_max = rtree_min = &dummy_node;			\
	for (;;) {						\
	    if ((key) < (root)->value) {			\
		if ((y = (root)->left) == NULL)			\
		    break;					\
		if ((key) < y->value) {				\
		    (root)->left = y->right;			\
		    y->right = (root);				\
		    (root) = y;					\
		    if ((y = (root)->left) == NULL)		\
			break;					\
		}						\
		rtree_min->left = (root);			\
		rtree_min = (root);				\
	    } else if ((key) > (root)->value) {			\
		if ((y = (root)->right) == NULL)		\
		    break;					\
		if ((key) > y->value) {				\
		    (root)->right = y->left;			\
		    y->left = (root);				\
		    (root) = y;					\
		    if ((y = (root)->right) == NULL)		\
			break;					\
		}						\
		ltree_max->right = (root);			\
		ltree_max = (root);				\
	    } else						\
		break;						\
	    (root) = y;						\
	}							\
	ltree_max->right = (root)->left;			\
	rtree_min->left = (root)->right;			\
	(root)->left = dummy_node.right;			\
	(root)->right = dummy_node.left;			\
    }

#endif  /* ! _REGULAR_SPLAY_TREE_ */
