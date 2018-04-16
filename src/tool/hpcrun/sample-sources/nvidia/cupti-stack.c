#include <lib/prof-lean/stdatomic.h>

#include "cupti-stack.h"
#include "cupti-node.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

//////////////////////////////////////
//            interfaces (concurrent)
//////////////////////////////////////
void
cupti_stack_init
(
 cupti_stack_t *stack
)
{
  atomic_init(&stack->head, NULL);
}


cupti_node_t *
cupti_stack_head
(
 cupti_stack_t *stack
)
{
  return atomic_load(&stack->head);
}


cupti_node_t *
cupti_stack_pop(cupti_stack_t *stack)
{
  cupti_node_t *first = atomic_load(&stack->head);
  if (first) {
    cupti_node_t *succ = first->next;
    while (!atomic_compare_exchange_strong(&(stack->head), &first, succ) && first) {
      succ = first->next;
    }
  }
  return first;
} 


void
cupti_stack_push
(
 cupti_stack_t *stack,
 cupti_node_t *node
)
{
  cupti_node_t *first = atomic_load(&stack->head);
  node->next = first;
  while (!atomic_compare_exchange_strong(&(stack->head), &(node->next), node));
}


void
cupti_stack_steal
(
 cupti_stack_t *stack,
 cupti_stack_t *other
)
{
  cupti_node_t *head = atomic_exchange(&other->head, NULL);
  atomic_store(&stack->head, head);
}


void
cupti_stack_apply
(
 cupti_stack_t *stack,
 cupti_stack_t *free_stack,
 cupti_stack_fn_t fn
)
{
  cupti_node_t *node;
  while ((node = cupti_stack_pop(stack)) != NULL) {
    fn(node);
    cupti_stack_push(free_stack, node);
  }
}
