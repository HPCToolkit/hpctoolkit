#ifndef _HPCTOOLKIT_CUPTI_STACK_H_
#define _HPCTOOLKIT_CUPTI_STACK_H_

#include <lib/prof-lean/stdatomic.h>
#include "cupti-node.h"


// stack type
typedef struct cupti_stack {
  _Atomic(cupti_node_t *) head;
} cupti_stack_t;


typedef void (*cupti_stack_fn_t)(cupti_node_t *node);


// generic functions
void
cupti_stack_init
(
 cupti_stack_t *stack
);


cupti_node_t *
cupti_stack_head
(
 cupti_stack_t *stack
);


cupti_node_t *
cupti_stack_pop
(
 cupti_stack_t *stack
);


void
cupti_stack_push
(
 cupti_stack_t *stack,
 cupti_node_t *node
);


void
cupti_stack_apply
(
 cupti_stack_t *stack,
 cupti_stack_t *free_stack,
 cupti_stack_fn_t fn
);


void
cupti_stack_steal
(
 cupti_stack_t *stack,
 cupti_stack_t *other
);

#endif
