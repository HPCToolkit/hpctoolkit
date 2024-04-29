#include "collections-util.h"

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>


/* Check if MPSC_QUEUE_PREFIX is defined */
#ifdef MPSC_QUEUE_PREFIX
  #define MPSC_QUEUE_MEMBER(NAME) JOIN2(MPSC_QUEUE_PREFIX, NAME)
#else
  #error MPSC_QUEUE_PREFIX is not defined
#endif


#if !defined(MPSC_QUEUE_DECLARE) && !defined(MPSC_QUEUE_DEFINE)
  #define MPSC_QUEUE_DEFINE_INPLACE
#endif


/* Define MPSC_QUEUE_TYPE */
#if defined(MPSC_QUEUE_DECLARE) || defined(MPSC_QUEUE_DEFINE_INPLACE)
  /* Check if MPSC_QUEUE_ENTRY_TYPE is defined */
  #ifndef MPSC_QUEUE_ENTRY_TYPE
    #error MPSC_QUEUE_ENTRY_TYPE is not defined
  #endif

  /* Define MPSC_QUEUE_TYPE */
  #define MPSC_QUEUE_TYPE MPSC_QUEUE_MEMBER(t)
  typedef struct MPSC_QUEUE_TYPE {
    _Atomic(MPSC_QUEUE_ENTRY_TYPE *) head;
    _Atomic(MPSC_QUEUE_ENTRY_TYPE *) tail;
  } MPSC_QUEUE_TYPE;

  #define MPSC_QUEUE_INITIALIZER { .head = ATOMIC_VAR_INIT(NULL),         \
                                   .tail = ATOMIC_VAR_INIT(NULL) }

  #ifdef MPSC_QUEUE_ALLOCATE
    #define MPSC_QUEUE_ALLOCATOR_TYPE MPSC_QUEUE_MEMBER(allocator_t)
    typedef MPSC_QUEUE_TYPE MPSC_QUEUE_ALLOCATOR_TYPE;
    #define MPSC_QUEUE_ALLOCATOR_INITIALIZER MPSC_QUEUE_INITIALIZER
  #endif
#endif


#ifdef MPSC_QUEUE_DECLARE

void
MPSC_QUEUE_MEMBER(init)
(
 MPSC_QUEUE_TYPE *queue
);


void
MPSC_QUEUE_MEMBER(enqueue)
(
 MPSC_QUEUE_TYPE *queue,
 MPSC_QUEUE_ENTRY_TYPE *entry
);


MPSC_QUEUE_ENTRY_TYPE *
MPSC_QUEUE_MEMBER(dequeue)
(
 MPSC_QUEUE_TYPE *queue
);


#ifdef MPSC_QUEUE_ALLOCATE
void
MPSC_QUEUE_MEMBER(init_allocator)
(
 MPSC_QUEUE_ALLOCATOR_TYPE *allocator
);


MPSC_QUEUE_ENTRY_TYPE *
MPSC_QUEUE_MEMBER(allocate)
(
 MPSC_QUEUE_ALLOCATOR_TYPE *allocator
);


void
MPSC_QUEUE_MEMBER(deallocate)
(
 MPSC_QUEUE_ENTRY_TYPE *entry
);
#endif

#endif



#if defined(MPSC_QUEUE_DEFINE) || defined(MPSC_QUEUE_DEFINE_INPLACE)

#ifdef MPSC_QUEUE_DEFINE_INPLACE
#define MPSC_QUEUE_FN_MOD __attribute__((unused)) static
#else
#define MPSC_QUEUE_FN_MOD
#endif


MPSC_QUEUE_FN_MOD
void
MPSC_QUEUE_MEMBER(init)
(
 MPSC_QUEUE_TYPE *queue
)
{
  atomic_init(&queue->head, NULL);
  atomic_init(&queue->tail, NULL);
}


MPSC_QUEUE_FN_MOD
void
MPSC_QUEUE_MEMBER(enqueue)
(
 MPSC_QUEUE_TYPE *queue,
 MPSC_QUEUE_ENTRY_TYPE *entry
)
{
  /* Just in case, invalidate the next pointer. */
  /* This can be non-atomic operation. */
  atomic_store(&entry->next, NULL);

  /* Atomically update the tail of an element and */
  /* determine whether the predecessor exists. */
  MPSC_QUEUE_ENTRY_TYPE *predecessor = atomic_exchange(&queue->tail, entry);
  if (predecessor) {
    /* If so, enqueue this element behind the predecessor. */
    atomic_store(&predecessor->next, entry);
  } else {
    /* Otherwise, the queue is a singleton, so update the head. */
    atomic_store(&queue->head, entry);
  }
}


MPSC_QUEUE_FN_MOD
MPSC_QUEUE_ENTRY_TYPE *
MPSC_QUEUE_MEMBER(dequeue)
(
 MPSC_QUEUE_TYPE *queue
)
{
  MPSC_QUEUE_ENTRY_TYPE *first = atomic_load(&queue->head);

  if (first) {
    /* The queue is non-empty. */
    MPSC_QUEUE_ENTRY_TYPE *next = atomic_load(&first->next);
    if (next) {
      /* Update head with successor. */
      /* The update may be relaxed because the next access to head */
      /* will be performed by the same dequeuing thread. */
      atomic_store(&queue->head, next);
    } else {
      if (atomic_compare_exchange_strong(&queue->tail, &first, 0)) {
        MPSC_QUEUE_ENTRY_TYPE *expected_head = first;
        /* When the previous CAS succeeded, */
        /* the queue was a singleton. */
        /* Replace the head with NULL, unless a subsequent enqueue */
        /* already updated head. */
        atomic_compare_exchange_strong(&queue->head, &expected_head, 0);
        } else {
          /* A successor behind the first is not yet linked, so we  */
          /* cannot return the first yet. */
          first = NULL;
        }
      }
    }

  /* Return the first element if its successor was known or */
  /* if the queue was a singleton. Otherwise, return NULL. */
  return first;
}


#ifdef MPSC_QUEUE_ALLOCATE

MPSC_QUEUE_FN_MOD
void
MPSC_QUEUE_MEMBER(init_allocator)
(
 MPSC_QUEUE_ALLOCATOR_TYPE *allocator
)
{
  MPSC_QUEUE_MEMBER(init)(allocator);
}


MPSC_QUEUE_FN_MOD
MPSC_QUEUE_ENTRY_TYPE *
MPSC_QUEUE_MEMBER(allocate)
(
 MPSC_QUEUE_ALLOCATOR_TYPE *allocator
)
{
  MPSC_QUEUE_ENTRY_TYPE *entry = MPSC_QUEUE_MEMBER(dequeue)(allocator);
  if (entry == NULL) {
    entry = MPSC_QUEUE_ALLOCATE(sizeof(MPSC_QUEUE_ENTRY_TYPE));
  }

  entry->allocator = allocator;

  return entry;
}


MPSC_QUEUE_FN_MOD
void
MPSC_QUEUE_MEMBER(deallocate)
(
 MPSC_QUEUE_ENTRY_TYPE *entry
)
{
  MPSC_QUEUE_MEMBER(enqueue)(entry->allocator, entry);
}
#endif

#endif
