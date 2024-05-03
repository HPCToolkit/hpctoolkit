// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#include "stacks.h"

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stddef.h>


//*****************************************************************************
// interface functions
//*****************************************************************************


#define Ad(q) q.aptr
#define Ap(q) q->aptr

void
sstack_ptr_set
(
 s_element_ptr_t *p,
 s_element_t *v
)
{
  atomic_store_explicit(&Ap(p), v, memory_order_relaxed);
}


s_element_t *
sstack_ptr_get
(
 s_element_ptr_t *e
)
{
  return (s_element_t *) atomic_load_explicit(&Ap(e), memory_order_relaxed);
}


s_element_t *
sstack_swap
(
 s_element_ptr_t *q,
 s_element_t *r
)
{
  return (s_element_t *) atomic_exchange_explicit(&Ap(q), r, memory_order_relaxed);
}


void
sstack_push
(
 s_element_ptr_t *q,
 s_element_t *e
)
{
  s_element_t *first =
    (s_element_t *) atomic_load_explicit(&Ap(q), memory_order_relaxed);

  atomic_store_explicit(&(e->Ad(next)), first, memory_order_relaxed);
  atomic_store_explicit(&Ap(q), e, memory_order_relaxed);
}


s_element_t *
sstack_pop
(
 s_element_ptr_t *q
)
{
  s_element_t *e = (s_element_t *) atomic_load_explicit(&Ap(q), memory_order_relaxed);
  if (e) {
    s_element_t *next =
      (s_element_t *) atomic_load_explicit(&(e->Ad(next)), memory_order_relaxed);
    atomic_store_explicit(&Ap(q), next, memory_order_relaxed);
    atomic_store_explicit(&(e->Ad(next)), 0, memory_order_relaxed);
  }
  return e;
}


s_element_t *
sstack_steal
(
 s_element_ptr_t *q
)
{
  s_element_t *e = sstack_swap(q, 0);

  return e;
}


void
sstack_reverse
(
 s_element_ptr_t *q
)
{
  s_element_t *prev = NULL;
  s_element_t *e = (s_element_t *) atomic_load_explicit(&Ap(q), memory_order_relaxed);
  while (e) {
    s_element_t *next =
      (s_element_t *) atomic_load_explicit(&(e->Ad(next)), memory_order_relaxed);
    atomic_store_explicit(&(e->Ad(next)), prev, memory_order_relaxed);
    prev = e;
    e = next;
  }
  atomic_store_explicit(&Ap(q), prev, memory_order_relaxed);
}


void
sstack_forall
(
 s_element_ptr_t *q,
 stack_forall_fn_t fn,
 void *arg
)
{
  s_element_t *current =
    (s_element_t *) atomic_load_explicit(&Ap(q), memory_order_relaxed);

  while (current) {
    fn(current, arg);
    current =
      (s_element_t *) atomic_load_explicit(&current->Ad(next), memory_order_relaxed);
  }
}


void
cstack_ptr_set
(
 s_element_ptr_t *e,
 s_element_t *v
)
{
  atomic_init(&Ap(e), v);
}


s_element_t *
cstack_ptr_get
(
 s_element_ptr_t *e
)
{
  return (s_element_t *) atomic_load(&Ap(e));
}


s_element_t *
cstack_swap
(
 s_element_ptr_t *q,
 s_element_t *r
)
{
  s_element_t *e = (s_element_t *) atomic_exchange(&Ap(q), r);

  return e;
}


void
cstack_push
(
 s_element_ptr_t *q,
 s_element_t *e
)
{
  s_element_t *head = (s_element_t *) atomic_load(&Ap(q));
  s_element_t *new_head = e;

  // push a singleton or a chain on the list
  for (;;) {
    s_element_t *enext = (s_element_t *) atomic_load(&e->Ad(next));
    if (enext == 0) break;
    e = enext;
  }

  do {
    atomic_store(&e->Ad(next), head);
  } while (!atomic_compare_exchange_strong(&Ap(q), &head, new_head));
}


s_element_t *
cstack_pop
(
 s_element_ptr_t *q
)
{
  s_element_t *oldhead = (s_element_t *) atomic_load(&Ap(q));
  s_element_t *next = 0;

  do {
    if (oldhead == 0) return 0;
    next = (s_element_t *) atomic_load(&oldhead->Ad(next));
  } while (!atomic_compare_exchange_strong(&Ap(q), &oldhead, next));

  atomic_store(&oldhead->Ad(next), 0);

  return oldhead;
}


s_element_t *
cstack_steal
(
 s_element_ptr_t *q
)
{
  s_element_t *e = cstack_swap(q, 0);

  return e;
}


void
cstack_forall
(
 s_element_ptr_t *q,
 stack_forall_fn_t fn,
 void *arg
)
{
  s_element_t *current = (s_element_t *) atomic_load(&Ap(q));
  while (current) {
    fn(current, arg);
    current = (s_element_t *) atomic_load(&current->Ad(next));
  }
}


//*****************************************************************************
// unit test
//*****************************************************************************

#define UNIT_TEST 0
#if UNIT_TEST

#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

typedef struct {
    s_element_ptr_t next;
    int value;
} typed_stack_elem(int); // int_q_element_t

typed_stack_declare_type(int);

typed_stack_impl(int, cstack);

typed_stack_elem_ptr(int) queue;

void
print(typed_stack_elem(int) *e, void *arg)
{
  printf("%d\n", e->value);
}

int main(int argc, char **argv)
{
  int i;
  for (i = 0; i < 10; i++) {
    typed_stack_elem_ptr(int)
      item = (typed_stack_elem_ptr(int)) malloc(sizeof(typed_stack_elem(int)));
    item->value = i;
    typed_stack_elem_ptr_set(int, cstack)(item, 0);
    typed_stack_push(int, cstack)(&queue, item);
  }
  typed_stack_forall(int, cstack)(&queue, print, 0);
}

#endif
