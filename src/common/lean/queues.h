// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef QUEUES_H
#define QUEUES_H

//*****************************************************************************
// Description:
//
//   interface for sequential and concurrent LIFO queues (AKA stacks)
//
//*****************************************************************************



//*****************************************************************************
// local includes
//*****************************************************************************

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif



//*****************************************************************************
// macros
//*****************************************************************************

// routine name for a queue operation
#define queue_op(qtype, op)                     \
  qtype ## _ ## op

// typed queue pointer
#define typed_queue_elem_ptr(type)              \
  type ## _ ## q_element_ptr_t

#define typed_queue_elem(type)                  \
  type ## _ ## q_element_t

#define typed_queue_elem_fn(type, fn)           \
  type ## _ ## q_element_ ## fn

// routine name for a typed queue operation
#define typed_queue_op(type, qtype, op)         \
  type ## _ ## qtype ## _ ## op

// ptr set routine name for a typed queue
#define typed_queue_elem_ptr_set(type, qtype)   \
  typed_queue_op(type, qtype, ptr_set)

// ptr get routine name for a typed queue
#define typed_queue_elem_ptr_get(type, qtype)   \
  typed_queue_op(type, qtype, ptr_get)

// swap routine name for a typed queue
#define typed_queue_swap(type, qtype)           \
  typed_queue_op(type, qtype, swap)

// push routine name for a typed queue
#define typed_queue_push(type, qtype)           \
  typed_queue_op(type, qtype, push)

// pop routine name for a typed queue
#define typed_queue_pop(type, qtype)            \
  typed_queue_op(type, qtype, pop)

// steal routine name for a typed queue
#define typed_queue_steal(type, qtype)          \
  typed_queue_op(type, qtype, steal)


// define typed wrappers for a queue type
#define typed_queue(type, qtype)                                \
  void                                                          \
  typed_queue_elem_ptr_set(type, qtype)                         \
    (typed_queue_elem_ptr(type) *e,                             \
     typed_queue_elem(type) *v)                                 \
  {                                                             \
    queue_op(qtype,ptr_set)((q_element_ptr_t *) e,              \
                            (q_element_t *) v);                 \
  }                                                             \
                                                                \
  typed_queue_elem(type) *                                      \
  typed_queue_elem_ptr_get(type, qtype)                         \
    (typed_queue_elem_ptr(type) *e)                             \
  {                                                             \
    typed_queue_elem(type) *r = (typed_queue_elem(type) *)      \
      queue_op(qtype,ptr_get)((q_element_ptr_t *) e);           \
    return r;                                                   \
  }                                                             \
                                                                \
  typed_queue_elem(type) *                                      \
  typed_queue_swap(type, qtype)                                 \
    (typed_queue_elem_ptr(type) *q, typed_queue_elem(type) *v)  \
  {                                                             \
    typed_queue_elem(type) *e = (typed_queue_elem(type) *)      \
      queue_op(qtype,swap)((q_element_ptr_t *) q,               \
                           (q_element_t *) v);                  \
    return e;                                                   \
  }                                                             \
                                                                \
  void                                                          \
  typed_queue_push(type, qtype)                                 \
    (typed_queue_elem_ptr(type) *q, typed_queue_elem(type) *e)  \
  {                                                             \
    queue_op(qtype,push)((q_element_ptr_t *) q,                 \
                         (q_element_t *) e);                    \
  }                                                             \
                                                                \
  typed_queue_elem(type) *                                      \
  typed_queue_pop(type, qtype)                                  \
  (typed_queue_elem_ptr(type) *q)                               \
  {                                                             \
    typed_queue_elem(type) *e = (typed_queue_elem(type) *)      \
      queue_op(qtype,pop)((q_element_ptr_t *) q);               \
    return e;                                                   \
  }                                                             \
                                                                \
  typed_queue_elem(type) *                                      \
  typed_queue_steal(type, qtype)                                \
  (typed_queue_elem_ptr(type) *q)                               \
  {                                                             \
    typed_queue_elem(type) *e = (typed_queue_elem(type) *)      \
      queue_op(qtype,steal)((q_element_ptr_t *) q);             \
    return e;                                                   \
  }


//*****************************************************************************
// types
//*****************************************************************************

typedef union q_element_ptr_u {
  struct q_element_s *ptr;
  _Atomic(struct q_element_s*) aptr;
} q_element_ptr_t;


typedef struct q_element_s {
  q_element_ptr_t next;
} q_element_t;



//*****************************************************************************
// interface functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// sequential LIFO queue interface operations
//-----------------------------------------------------------------------------

void
squeue_ptr_set
(
 q_element_ptr_t *e,
 q_element_t *v
);


q_element_t *
squeue_ptr_get
(
 q_element_ptr_t *e
);


// set q->next to point to e and return old value of q->next
q_element_t *
squeue_swap
(
  q_element_ptr_t *q,
  q_element_t *e
);


// push a singleton e or a chain beginning with e onto q
void
squeue_push
(
  q_element_ptr_t *q,
  q_element_t *e
);


// pop a singlegon from q or return 0
q_element_t *
squeue_pop
(
  q_element_ptr_t *q
);


// steal the entire chain rooted at q
q_element_t *
squeue_steal
(
  q_element_ptr_t *q
);



//-----------------------------------------------------------------------------
// concurrent LIFO queue interface operations
//-----------------------------------------------------------------------------

void
cqueue_ptr_set
(
 q_element_ptr_t *e,
 q_element_t *v
);


q_element_t *
cqueue_ptr_get
(
 q_element_ptr_t *e
);


// set q->next to point to e and return old value of q->next
q_element_t *
cqueue_swap
(
  q_element_ptr_t *q,
  q_element_t *e
);


// push a singleton e or a chain beginning with e onto q
void
cqueue_push
(
  q_element_ptr_t *q,
  q_element_t *e
);


// pop a singlegon from q or return 0
q_element_t *
cqueue_pop
(
  q_element_ptr_t *q
);


// steal the entire chain rooted at q
q_element_t *
cqueue_steal
(
  q_element_ptr_t *q
);

#endif
