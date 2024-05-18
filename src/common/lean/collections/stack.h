#include "collections-util.h"

#include <stdlib.h>
#include <stdbool.h>


/* Check if stack prefix is defined */
#ifdef STACK_PREFIX
  #define STACK_MEMBER(NAME) JOIN2(STACK_PREFIX, NAME)
#else
  #error STACK_PREFIX is not defined
#endif


/* Define stack type */
#if defined(STACK_DECLARE) || defined(STACK_DEFINE_INPLACE)

  /* Check if STACK_ENTRY_TYPE is defined */
  #if !defined(STACK_ENTRY_TYPE)
    #error STACK_ENTRY_TYPE is not defined
  #endif

  /* Define STACK_TYPE */
  #define STACK_TYPE STACK_MEMBER(t)
  typedef struct STACK_TYPE {
    STACK_ENTRY_TYPE *head;
  } STACK_TYPE;

#endif


#if defined(STACK_DECLARE)

void
STACK_MEMBER(init_stack)
(
 STACK_TYPE *stack
);


void
STACK_MEMBER(init_entry)
(
 STACK_ENTRY_TYPE *entry
);


void
STACK_MEMBER(push)
(
 STACK_TYPE *stack,
 STACK_ENTRY_TYPE *entry
);


STACK_ENTRY_TYPE *
STACK_MEMBER(pop)
(
 STACK_TYPE *stack
);


STACK_ENTRY_TYPE *
STACK_MEMBER(top)
(
 STACK_TYPE *stack
);


bool
STACK_MEMBER(empty)
(
 const STACK_TYPE *stack
);


void
STACK_MEMBER(for_each_entry)
(
 STACK_TYPE *stack,
 void (*fn)(STACK_ENTRY_TYPE *, void *),
 void *arg
);

#endif


#if defined(STACK_DEFINE) || defined(STACK_DEFINE_INPLACE)

#ifdef STACK_DEFINE_INPLACE
#define STACK_FN_MOD static
#else
#define STACK_FN_MOD
#endif


STACK_FN_MOD void
STACK_MEMBER(init_stack)
(
 STACK_TYPE *stack
)
{
  stack->head = NULL;
}



STACK_FN_MOD void
STACK_MEMBER(init_entry)
(
 STACK_ENTRY_TYPE *entry
)
{
  entry->next = NULL;
}


STACK_FN_MOD void
STACK_MEMBER(push)
(
 STACK_TYPE *stack,
 STACK_ENTRY_TYPE *entry
)
{
  entry->next = stack->head;
  stack->head = entry;
}


STACK_FN_MOD STACK_ENTRY_TYPE *
STACK_MEMBER(pop)
(
 STACK_TYPE *stack
)
{
  STACK_ENTRY_TYPE *ret = stack->head;

  if (ret != NULL) {
    stack->head = ret->next;
    ret->next = NULL;
  }

  return ret;
}


STACK_FN_MOD STACK_ENTRY_TYPE *
STACK_MEMBER(top)
(
 STACK_TYPE *stack
)
{
  return stack->head;
}


STACK_FN_MOD bool
STACK_MEMBER(empty)
(
 const STACK_TYPE *stack
)
{
  return stack->head == NULL;
}


STACK_FN_MOD void
STACK_MEMBER(for_each_entry)
(
 STACK_TYPE *stack,
 void (*fn)(STACK_ENTRY_TYPE *, void *),
 void *arg
)
{
  STACK_ENTRY_TYPE *curr = stack->head;

  while (curr != NULL) {
    fn(curr, arg);
    curr = curr->next;
  }
}

#endif


#undef STACK_PREFIX
#undef STACK_MEMBER
#undef STACK_TYPE
#undef STACK_ENTRY_TYPE
#undef STACK_DECLARE
#undef STACK_DEFINE
#undef STACK_DEFINE_INPLACE
