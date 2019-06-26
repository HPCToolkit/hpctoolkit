#include <lib/prof-lean/stdatomic.h>

#include "cstack.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

//////////////////////////////////////
//            interfaces (concurrent)
//////////////////////////////////////
void
cstack_init
(
 cstack_t *stack
)
{
  atomic_init(&stack->head, NULL);
}


cstack_node_t *
cstack_head
(
 cstack_t *stack
)
{
  return atomic_load(&stack->head);
}


cstack_node_t *
cstack_pop(cstack_t *stack)
{
  cstack_node_t *first = atomic_load(&stack->head);
  if (first) {
    cstack_node_t *succ = first->next;
    while (!atomic_compare_exchange_strong(&(stack->head), &first, succ) && first) {
      succ = first->next;
    }
  }
  return first;
} 


void
cstack_push
(
 cstack_t *stack,
 cstack_node_t *node
)
{
  cstack_node_t *first = atomic_load(&stack->head);
  node->next = first;
  while (!atomic_compare_exchange_strong(&(stack->head), &(node->next), node));
}


void
cstack_steal
(
 cstack_t *stack,
 cstack_t *other
)
{
  cstack_node_t *head = atomic_exchange(&other->head, NULL);
  atomic_store(&stack->head, head);
}


void
cstack_apply
(
 cstack_t *stack,
 cstack_t *free_stack,
 cstack_fn_t fn
)
{
  cstack_node_t *node;
  while ((node = cstack_pop(stack)) != NULL) {
    fn(node);
    if (free_stack != NULL) {
      cstack_push(free_stack, node);
    }
  }
}


void
cstack_apply_iterate
(
 cstack_t *stack,
 cstack_fn_t fn
)
{
  cstack_node_t *node = atomic_load(&stack->head);
  while (node != NULL) {
    fn(node);
    node = node->next;
  }
}
