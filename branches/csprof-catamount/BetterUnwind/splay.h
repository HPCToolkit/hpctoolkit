/*
 * Declarations used by interval splay tree code.
 *
 * $Id$
 */

#ifndef _SPLAY_H_
#define _SPLAY_H_

#include "intervals.h"

/*
 * As a slicky trick, interval_tree_node overlays with
 * unwind_interval_t where next = right and prev = left.
 * (May not want to keep this.)
 */

#ifdef DUPLICATE_CODE
struct interval_tree_node {
    unsigned long start;
    unsigned long end;
    ra_loc ra_status;
    unsigned int ra_pos;
    struct interval_tree_node *right;
    struct interval_tree_node *left;
};
#else
#define interval_tree_node unwind_interval_t

#define START(n) n->startaddr
#define END(n)   n->endaddr
#define RIGHT(n) n->next
#define LEFT(n)  n->prev

#define SRIGHT(n) n.next
#define SLEFT(n)  n.prev
#endif

typedef struct interval_tree_node *interval_tree_node_t;

void csprof_interval_tree_init(void);
unwind_interval *csprof_addr_to_interval(unsigned long addr);

#define SUCCESS  0
#define FAILURE  1

#endif  /* !_SPLAY_H_ */
