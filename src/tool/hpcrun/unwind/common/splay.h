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

#define START(n) n->start
#define END(n)   n->end
#define RIGHT(n) n->next
#define LEFT(n)  n->prev

#define SRIGHT(n) n.next
#define SLEFT(n)  n.prev

// FIXME: STOP embedding pointer info in type!! 
//
typedef interval_tree_node *interval_tree_node_t;


extern void                csprof_interval_tree_init(void);
extern void                csprof_release_splay_lock(void);
extern interval_tree_node *csprof_addr_to_interval(void *addr);

extern void             csprof_print_interval_tree(void);

#define SUCCESS  0
#define FAILURE  1

#endif  /* !_SPLAY_H_ */
