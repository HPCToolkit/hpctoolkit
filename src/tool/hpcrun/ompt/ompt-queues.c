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

#include "ompt-queues.h"



//*****************************************************************************
// interface functions
//*****************************************************************************

void
squeue_push
(
  q_element_t **q,
  q_element_t *e
)
{
  e->next = *q;
  *q = e;
}


q_element_t *
squeue_pop
(
  q_element_t **q
)
{
  q_element_t *e = 0;

  if (*q) {
    e = *q;
    *q = e->next;
    e->next = 0;
  } 

  return e;
}

#if 0
// vi3: wait free queue functions
// prefix wfq
void
wfq_set_next_pending
(
 ompt_base_t *element
)
{
  // set invalid value
  atomic_store(&element->next.anext, ompt_base_invalid);
}

// prefix wfq
ompt_base_t*
wfq_get_next
(
 ompt_base_t *element
)
{
  ompt_base_t* next;

  // wait until next pointer is properly initialized
  for(;;) {
    next = atomic_load(&element->next.anext);
    if (next != ompt_base_invalid) break;
  }

  return next;
}


void
wfq_init
(
ompt_wfq_t *queue
)
{
  atomic_init(&queue->head, ompt_base_nil);
}


void
wfq_enqueue
(
 ompt_base_t *new, 
 ompt_wfq_t *queue
)
{
  wfq_set_next_pending(new);
  // FIXME vi3: should OMPT_BASE_T_STAR
  ompt_base_t *old_head = (ompt_base_t*)atomic_exchange(&queue->head, new);
  atomic_store(&new->next.anext, old_head);
}

// remove one element from the end and is used in case
// when all adding are finished before removing
// dequeue_public
ompt_base_t*
wfq_dequeue_public
(
 ompt_wfq_t *public_queue
)
{
  ompt_base_t* first = atomic_load(&public_queue->head);
  if (first){
    ompt_base_t* succ = wfq_get_next(first);
    atomic_store(&public_queue->head, succ);
    atomic_store(&first->next.anext, ompt_base_nil);
  }
  return first;
}

// returns first element from private list
// if it is empty, takes all element from public queue
// and store them to private list
ompt_base_t*
wfq_dequeue_private
(
 ompt_wfq_t *public_queue, 
 ompt_base_t **private_queue
)
{
  // private list is empty
  if (!(*private_queue)){
    // take all from public list, but there is also possibility that this list is empty too
    ompt_base_t* old_head = atomic_exchange(&public_queue->head, ompt_base_nil);
    *private_queue = old_head;
  }

  // In private_queue_remove_first, thread will spinwait while
  // the next pointer of the current head is invalid
  // FIXME vi3: if it would be more elegant, we could spinwait in this function instead
  // of private_queue_remove_first
  return private_queue_remove_first(private_queue);
}

#endif


//*****************************************************************************
// test case
//*****************************************************************************

#define UNIT_TEST 1
#if UNIT_TEST

#include <stdlib.h>
#include <stdio.h>

typedef struct {
  q_element_t qe;
  int value;
} int_q_element_t;

int_q_element_t *queue = 0;

typed_squeue(int_q_element_t)

int_q_element_t *
int_q_element_new(int value)
{
  int_q_element_t *e =(int_q_element_t *) malloc(sizeof(int_q_element_t));
  e->value = value;
}


void pop(int n)
{
  for(int i = 0; i < n; i++) {
    int_q_element_t *e = int_q_element_t_squeue_pop(&queue);
    if (e == 0) {
      printf("queue empty\n");
      break;
    } else {
      printf("popping %d\n", e->value);
    }
  }
}


void 
push(int min, int n) {
  for(int i = min; i < min+n; i++) {
    printf("pushing %d\n", i);
    int_q_element_t_squeue_push(&queue, int_q_element_new(i));
  }
}


int
main(int argc, char **argv)
{
  push(0, 30);
  pop(10);
  push(100, 12);
  pop(100);
}

#endif
