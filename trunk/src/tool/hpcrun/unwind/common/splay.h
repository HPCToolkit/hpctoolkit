/*
 * Declarations used by interval splay tree code.
 *
 * $Id$
 */

#ifndef _SPLAY_H_
#define _SPLAY_H_

#include "splay-interval.h"


/*
 * As a slicky trick, interval_tree_node overlays with
 * unwind_interval_t where next = right and prev = left.
 * (May not want to keep this.)
 */

typedef struct splay_interval_s interval_tree_node;

#define START(n)   (n)->start
#define END(n)     (n)->end
#define RIGHT(n)   (n)->next
#define LEFT(n)    (n)->prev

#define SSTART(n)  (n).start
#define SEND(n)    (n).end
#define SRIGHT(n)  (n).next
#define SLEFT(n)   (n).prev

// FIXME: STOP embedding pointer info in type!! 
//
typedef interval_tree_node *interval_tree_node_t;

interval_tree_node *interval_tree_lookup(interval_tree_node **root, void *addr);
int  interval_tree_insert(interval_tree_node **root, interval_tree_node *node);
void interval_tree_delete(interval_tree_node **root, interval_tree_node **del_tree,
			  void *start, void *end);
void interval_tree_verify(interval_tree_node *root, const char *label);


#define SUCCESS  0
#define FAILURE  1

#endif  /* !_SPLAY_H_ */
