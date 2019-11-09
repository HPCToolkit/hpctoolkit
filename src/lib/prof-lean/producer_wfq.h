//
// Created by ax4 on 7/31/19.
//

#include "stdatomic.h"

#ifndef HPCTOOLKIT_PRODUCER_WFQ_H
#define HPCTOOLKIT_PRODUCER_WFQ_H

#define ignore(x) ;
#define show(x) x

#define typed_producer_wfq_declare(type) \
typed_producer_wfq_functions(type, ignore)

#define typed_producer_wfq_impl(type) \
typed_producer_wfq_functions(type, show)

#define producer_wfq_op(op)			\
  producer_wfq_ ## op

// routine name for a typed unordered_stack operation
#define typed_producer_wfq_op(type, op)		\
   type ## _producer_wfq_ ## op

#define typed_producer_wfq(type)  \
  type ## _ ## producer_wfq_t

#define typed_producer_wfq_init(type) \
    typed_producer_wfq_op(type, init)

#define typed_producer_wfq_enqueue(type)		\
  typed_producer_wfq_op(type, enqueue)

#define typed_producer_wfq_dequeue(type)		\
  typed_producer_wfq_op(type, dequeue)

#define typed_producer_wfq_elem(type)			\
  type ## _ ## producer_wfq_element_t


#define typed_producer_wfq_functions(type, macro)				\
\
\
  void                        \
  typed_producer_wfq_init(type)   \
    (typed_producer_wfq(type) *p)   \
    macro({ \
        producer_wfq_op(init) ((producer_wfq_t *) p); \
    }) \
  								\
 \
  void								\
  typed_producer_wfq_enqueue(type)					\
    (typed_producer_wfq(type) *q, typed_producer_wfq_elem(type) *e)	\
  macro({								\
        producer_wfq_op(enqueue) ((producer_wfq_t *) q,			\
			 (producer_wfq_element_t *) e);			\
  })								\
  								\
  typed_producer_wfq_elem(type) *					\
  typed_producer_wfq_dequeue(type)					\
  (typed_producer_wfq(type) *q)				\
  macro({								\
    typed_producer_wfq_elem(type) *e = (typed_producer_wfq_elem(type) *)	\
      producer_wfq_op(dequeue) ((producer_wfq_t *) q);		\
    return e;							\
  }) \



typedef struct producer_wfq_element_ptr_u {
  _Atomic(struct producer_wfq_element_s*) aptr;
} producer_wfq_element_ptr_t;

typedef struct producer_wfq_s {
  producer_wfq_element_ptr_t head;
  producer_wfq_element_ptr_t tail;
} producer_wfq_t;

typedef struct producer_wfq_element_s {
  producer_wfq_element_ptr_t next;
} producer_wfq_element_t;


void
producer_wfq_init
(
 producer_wfq_t *wfq
);


void
producer_wfq_ptr_set
(
 producer_wfq_element_ptr_t *e,
 producer_wfq_element_t *v
);


void
producer_wfq_enqueue
(
 producer_wfq_t *wfq,
 producer_wfq_element_t *e
);


producer_wfq_element_t*
producer_wfq_dequeue
(
 producer_wfq_t *wfq
);

#endif //HPCTOOLKIT_PRODUCER_WFQ_H
