#include "collections-util.h"

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>


/* Check if CONCURRENT_STACK_PREFIX is defined */
#ifdef CONCURRENT_STACK_PREFIX
  #define CONCURRENT_STACK_MEMBER(NAME) JOIN2(CONCURRENT_STACK_PREFIX, NAME)
#else
  #error CONCURRENT_STACK_PREFIX is not defined
#endif


#if !defined(CONCURRENT_STACK_DECLARE) && !defined(CONCURRENT_STACK_DEFINE)
#define CONCURRENT_STACK_DEFINE_INPLACE
#endif


/* Define CONCURRENT_STACK_TYPE */
#if defined(CONCURRENT_STACK_DECLARE) || defined(CONCURRENT_STACK_DEFINE_INPLACE)
  /* Check if CONCURRENT_STACK_ENTRY_TYPE is defined */
  #ifndef CONCURRENT_STACK_ENTRY_TYPE
    #error CONCURRENT_STACK_ENTRY_TYPE is not defined
  #endif

  /* Define CONCURRENT_STACK_TYPE */
  #define CONCURRENT_STACK_TYPE CONCURRENT_STACK_MEMBER(t)
  typedef struct CONCURRENT_STACK_TYPE {
    _Atomic(CONCURRENT_STACK_ENTRY_TYPE *) head;
  } CONCURRENT_STACK_TYPE;
#endif


#ifdef CONCURRENT_STACK_DECLARE

void
CONCURRENT_STACK_MEMBER(init)
(
 CONCURRENT_STACK_TYPE *stack
);


void
CONCURRENT_STACK_MEMBER(push)
(
 CONCURRENT_STACK_TYPE *stack,
 CONCURRENT_STACK_ENTRY_TYPE *entry
);


CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(pop)
(
 CONCURRENT_STACK_TYPE *stack
);


CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(top)
(
 CONCURRENT_STACK_TYPE *stack
);


CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(pop_all)
(
 CONCURRENT_STACK_TYPE *stack
);


CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(next_node)
(
 CONCURRENT_STACK_ENTRY_TYPE *stack
);


void
CONCURRENT_STACK_MEMBER(for_each)
(
 CONCURRENT_STACK_TYPE *stack,
 void (*fn)(CONCURRENT_STACK_ENTRY_TYPE *, void *),
 void *arg
);

#endif


#if defined(CONCURRENT_STACK_DEFINE) || defined(CONCURRENT_STACK_DEFINE_INPLACE)

#ifdef CONCURRENT_STACK_DEFINE_INPLACE
#define CONCURRENT_STACK_FN_MOD __attribute__((unused)) static
#else
#define CONCURRENT_STACK_FN_MOD
#endif


CONCURRENT_STACK_FN_MOD
void
CONCURRENT_STACK_MEMBER(init)
(
 CONCURRENT_STACK_TYPE *stack
)
{
  atomic_init(&stack->head, NULL);
}


CONCURRENT_STACK_FN_MOD
void
CONCURRENT_STACK_MEMBER(push)
(
 CONCURRENT_STACK_TYPE *stack,
 CONCURRENT_STACK_ENTRY_TYPE *entry
)
{
  CONCURRENT_STACK_ENTRY_TYPE *old_head = atomic_load(&stack->head);
  do {
    atomic_store(&entry->next, old_head);
  } while (!atomic_compare_exchange_strong(&stack->head, &old_head, entry));
}


CONCURRENT_STACK_FN_MOD
CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(pop)
(
 CONCURRENT_STACK_TYPE *stack
)
{
  CONCURRENT_STACK_ENTRY_TYPE *old_head = atomic_load(&stack->head);
  CONCURRENT_STACK_ENTRY_TYPE *new_head;

  do {
    if (old_head == NULL) { return NULL; }
    new_head = atomic_load(&old_head->next);
  } while (!atomic_compare_exchange_strong(&stack->head, &old_head, new_head));

  atomic_store(&old_head->next, NULL);

  return old_head;
}


CONCURRENT_STACK_FN_MOD
CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(top)
(
 CONCURRENT_STACK_TYPE *stack
)
{
  return atomic_load(&stack->head);
}


CONCURRENT_STACK_FN_MOD
CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(pop_all)
(
 CONCURRENT_STACK_TYPE *stack
)
{
  return atomic_exchange(&stack->head, NULL);
}


CONCURRENT_STACK_FN_MOD
CONCURRENT_STACK_ENTRY_TYPE *
CONCURRENT_STACK_MEMBER(next_node)
(
 CONCURRENT_STACK_ENTRY_TYPE *stack
)
{
  return atomic_load(&stack->next);
}


CONCURRENT_STACK_FN_MOD
void
CONCURRENT_STACK_MEMBER(for_each)
(
 CONCURRENT_STACK_TYPE *stack,
 void (*fn)(CONCURRENT_STACK_ENTRY_TYPE *, void *),
 void *arg
)
{
  CONCURRENT_STACK_ENTRY_TYPE *curr = atomic_load(&stack->head);
  while (curr != NULL) {
    fn(curr, arg);
    curr = atomic_load(&curr->next);
  }
}

#endif
