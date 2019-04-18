// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//*****************************************************************************
// local includes 
//*****************************************************************************

#include "queues.h"



//*****************************************************************************
// interface functions
//*****************************************************************************

#define S(next) next.ptr
#define A(next) next.aptr

void
squeue_init
(
 q_element_t *q
)
{
  q->S(next) = 0;
}


q_element_t *
squeue_swap
(
 q_element_t *q,
 q_element_t *r
)
{
  q_element_t *e = q->S(next);
  q->S(next) = r;

  return e;
}


void
squeue_push
(
  q_element_t *q,
  q_element_t *e
)
{
  e->S(next) = q->S(next);
  q->S(next) = e;
}


q_element_t *
squeue_pop
(
  q_element_t *q
)
{
  q_element_t *e = 0;

  if (q->S(next)) {
    e = q->S(next);
    q->S(next) = e->S(next);
    e->S(next) = 0;
  } 

  return e;
}


q_element_t *
squeue_steal
(
  q_element_t *q
)
{
  q_element_t *e = squeue_swap(q, 0);

  return e;
}


void
cqueue_init
(
 q_element_t *q
)
{
  atomic_init(&q->A(next), 0);
}



q_element_t *
cqueue_swap
(
 q_element_t *q,
 q_element_t *r
)
{
  q_element_t *e = atomic_exchange(&q->A(next),r);

  return e;
}

void
cqueue_push
(
  q_element_t *q,
  q_element_t *e
)
{
  q_element_t *head = atomic_load(&q->A(next));
  q_element_t *new_head = e;

  // push a singleton or a chain on the list
  for (;;) {
    q_element_t *enext = atomic_load(&e->A(next));
    if (enext == 0) break;
    e = enext;
  } 

  do {
    atomic_store(&e->A(next), head);
  } while (!atomic_compare_exchange_strong(&q->A(next), &head, new_head));
}


q_element_t *
cqueue_pop
(
  q_element_t *q
)
{
  q_element_t *oldhead = atomic_load(&q->A(next));
  q_element_t *next = 0;

  do {
    if (oldhead == 0) return 0;
    next = atomic_load(&oldhead->A(next));
  } while (!atomic_compare_exchange_strong(&q->A(next), &oldhead, next));

  atomic_store(&oldhead->A(next),0);

  return oldhead;
}


q_element_t *
cqueue_steal
(
  q_element_t *q
)
{
  q_element_t *e = cqueue_swap(q, 0);

  return e;
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
  q_element_t next;
  int value;
} int_q_element_t;

int_q_element_t queue;

#define qtype cqueue
typed_queue(int_q_element_t, qtype)

int_q_element_t *
int_q_element_new(int value)
{
  int_q_element_t *e =(int_q_element_t *) malloc(sizeof(int_q_element_t));
  e->value = value;
  e->next.next.ptr = 0;
}


void 
pop
(
  int n
)
{
  int i;
  for(i = 0; i < n; i++) {
    int_q_element_t *e = typed_queue_pop(int_q_element_t,qtype)(&queue);
    if (e == 0) {
      printf("%d queue empty\n", omp_get_thread_num());
      break;
    } else {
      printf("%d popping %d\n", omp_get_thread_num(), e->value);
    }
  }
}


void 
push
(
  int min, 
  int n
) 
{
  int i;
  for(i = min; i < min+n; i++) {
    printf("%d pushing %d\n", omp_get_thread_num(), i);
    typed_queue_push(int_q_element_t, qtype)(&queue, int_q_element_new(i));
  }
}


void 
dump
(
  int_q_element_t *e
) 
{
  int i;
  for(; e; e = (int_q_element_t *) e->next.next.ptr) {
    printf("%d stole %d\n", omp_get_thread_num(), e->value);
  }
}


int
main
(
  int argc, 
  char **argv
)
{
  typed_queue_init(int_q_element_t,qtype)(&queue);
#pragma omp parallel
  {
  push(0, 30);
  pop(10);
  push(100, 12);
  // pop(100);
  int_q_element_t *e = typed_queue_steal(int_q_element_t,qtype)(&queue);
  dump(e);
  push(300, 30);
  typed_queue_push(int_q_element_t, qtype)(&queue, e);
  pop(100);
  }
}

#endif
