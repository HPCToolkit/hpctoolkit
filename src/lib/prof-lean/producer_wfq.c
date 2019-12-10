//
// Created by ax4 on 7/31/19.
//

#include <stdbool.h>

#include "producer_wfq.h"

#define Ad(wfq) wfq.aptr
#define Ap(wfq) wfq->aptr

void
producer_wfq_init
(
 producer_wfq_t *wfq
)
{
  atomic_store(&Ad(wfq->head), 0);
  atomic_store(&Ad(wfq->tail), 0);
}


void
producer_wfq_ptr_set
(
 producer_wfq_element_ptr_t *e,
 producer_wfq_element_t *v
)
{
  atomic_init(&Ap(e), v);
}


void
producer_wfq_enqueue
(
 producer_wfq_t *wfq,
 producer_wfq_element_t *e
)
{
  atomic_store_explicit(&Ad(e->next), 0, memory_order_relaxed);
  producer_wfq_element_t *last = atomic_exchange(&Ad(wfq->tail), e);
  if (!last) {
    atomic_store(&Ad(wfq->head), e);
  } else {
    atomic_store(&Ad(last->next), e);
  }
}


producer_wfq_element_t*
producer_wfq_dequeue
(
 producer_wfq_t *wfq
)
{
  producer_wfq_element_t *first, *second;
  while (true) {
    first = atomic_exchange(&Ad(wfq->head), 0);
    if (first) break;
    if (!atomic_load(&Ad(wfq->tail))) return 0;
  }

  second = atomic_load(&Ad(first->next));
  if (second) {
    atomic_store(&Ad(wfq->head), second);
  } else {
    if (!atomic_compare_exchange_strong(&Ad(wfq->tail), &first, 0)) {
      while (!atomic_load(&Ad(first->next)));
      producer_wfq_element_t *next = atomic_load(&Ad(first->next));
      atomic_store(&Ad(wfq->head), next);
    }
  }

  return first;
}


#define UNIT_TEST 0
#if UNIT_TEST

#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <unistd.h>

typedef struct {
    producer_wfq_element_ptr_t next;
    int value;
} typed_producer_wfq_elem(int); //int_q_element_t

typedef producer_wfq_t typed_producer_wfq(int);

//typed_queue_elem_ptr(int) queue;
typed_producer_wfq(int) wfq;

typed_producer_wfq_impl(int)

typed_producer_wfq_elem(int) *
new_elem(int value)
{
    typed_producer_wfq_elem(int) *e =(typed_producer_wfq_elem(int)* ) malloc(sizeof(int_producer_wfq_element_t));
    e->value = value;
    atomic_store(&Ad(e->next), 0);
}


void
dequeue
        (
                int n
        )
{
    int i;
    for(i = 0; i < n; i++) {
        typed_producer_wfq_elem(int) *e = typed_producer_wfq_dequeue(int)(&wfq);
        if (e == 0) {
            printf("%d queue empty\n", omp_get_thread_num());
            break;
        } else {
            printf("%d popping %d\n", omp_get_thread_num(), e->value);
        }
    }
}


void
enqueue
        (
                int min,
                int n
        )
{
    int i;
    for(i = min; i < min+n; i++) {
        printf("%d pushing %d\n", omp_get_thread_num(), i);
        typed_producer_wfq_enqueue(int)(&wfq, new_elem(i));
    }
}




int
main
        (
                int argc,
                char **argv
        )
{
    producer_wfq_init(&wfq);
#pragma omp parallel num_threads(2)
    {
        if (omp_get_thread_num() == 0 ) enqueue(0, 30);
        if (omp_get_thread_num() == 1 ) {
            sleep(1);
            dequeue(10);
        }
        if (omp_get_thread_num() == 0 ) enqueue(100, 12);
        // pop(100);
       // int_u_s_element_t *e = typed_unordered_stack_steal(int, qtype)(&queue);
        //dump(e);
        if (omp_get_thread_num() == 0 ) enqueue(300, 30);
        //typed_queue_
        if (omp_get_thread_num() == 1 ) {
            sleep(1);
            dequeue(70);
        }
    }
  //  enqueue(0, 2);
}

#endif



