#ifndef _HPCTOOLKIT_CSTACK_H_
#define _HPCTOOLKIT_CSTACK_H_

#include <lib/prof-lean/stdatomic.h>

typedef enum {
  CSTACK_TYPE_CUPTI_ACTIVITY = 0,
  CSTACK_TYPE_CUPTI_NOTIFICATION = 1,
  CSTACK_TYPE_SANITIZER_NOTIFICATION = 2,
  CSTACK_TYPE_SANITIZER_BUFFER = 3,
  CSTACK_TYPE_COUNT = 4
} cstack_entry_type_t;


// stack node with generic entry
typedef struct cstack_node {
  struct cstack_node *next;
  void *entry;
  cstack_entry_type_t type;
} cstack_node_t;

// stack type
typedef struct cstack {
  _Atomic(cstack_node_t *) head;
} cstack_t;

typedef void (*cstack_fn_t)(cstack_node_t *node);


// generic functions
void
cstack_init
(
 cstack_t *stack
);


cstack_node_t *
cstack_head
(
 cstack_t *stack
);


cstack_node_t *
cstack_pop
(
 cstack_t *stack
);


void
cstack_push
(
 cstack_t *stack,
 cstack_node_t *node
);


void
cstack_apply
(
 cstack_t *stack,
 cstack_t *free_stack,
 cstack_fn_t fn
);


void
cstack_apply_iterate
(
 cstack_t *stack,
 cstack_fn_t fn
);


void
cstack_steal
(
 cstack_t *stack,
 cstack_t *other
);

#endif
