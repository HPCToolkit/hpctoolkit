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
// Copyright ((c)) 2002-2020, Rice University
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

#include "bistack.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define Ap(s) &s.aptr



//*****************************************************************************
// interface functions
//*****************************************************************************

void 
bistack_init
(
 bistack_t *s
)
{
  atomic_init(Ap(s->produced), 0);
  atomic_init(Ap(s->to_consume), 0);
}


void
bistack_push
(
 bistack_t *s,
 s_element_t *e
)
{
  cstack_push(&s->produced, e);
}


s_element_t *
bistack_pop
(
 bistack_t *s
)
{
  // use sstack protocol for private consumer stack
  s_element_t *e = sstack_pop(&s->to_consume);

  return e;
}


void
bistack_reverse
(
 bistack_t *s
)
{
  sstack_reverse(&s->to_consume);
}


void
bistack_steal
(
 bistack_t *s
)
{
  if (atomic_load_explicit(Ap(s->produced), memory_order_relaxed) != NULL) {
    s_element_t *tmp = cstack_steal(&s->produced);
    atomic_store_explicit(Ap(s->to_consume), tmp, memory_order_relaxed);
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
#include <unistd.h>

typedef struct {
  s_element_ptr_t next;
  int value;
} typed_stack_elem(int); //int_q_element_t

typedef s_element_ptr_t typed_stack_elem_ptr(int);	 //int_q_elem_ptr_t
typedef bistack_t typed_bistack(int);

//typed_queue_elem_ptr(int) queue;
typed_bistack(int) pair;

typed_bistack_impl(int)

typed_stack_elem(int) *
typed_stack_elem_fn(int,new)(int value)
{
  typed_stack_elem(int) *e = 
    (typed_stack_elem(int)* ) malloc(sizeof(int_s_element_t));
  e->value = value;
  cstack_ptr_set(&e->next, 0);
}


void
pop
(
 int n
)
{
  int i;
  for(i = 0; i < n; i++) {
    typed_stack_elem(int) *e = typed_bistack_pop(int)(&pair);
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
  for(i = min; i < min + n; i++) {
    printf("%d pushing %d\n", omp_get_thread_num(), i);
    typed_bistack_push(int)(&pair, typed_stack_elem_fn(int, new)(i));
  }
}


void
steal
(
)
{
  typed_bistack_steal(int)(&pair);
}


#ifdef DUMP_UNORDERED_STACK
void
dump
(
 int_s_element_t *e
)
{
  int i;
  for(; e; 
      e = (int_s_element_t *) typed_stack_elem_ptr_get(int,cstack)(&e->next)) {
    printf("%d stole %d\n", omp_get_thread_num(), e->value);
  }
}

#endif


int
main
(
 int argc,
 char **argv
)
{
  bistack_init(&pair);
#pragma omp parallel num_threads(6)
  {
    if (omp_get_thread_num() != 5 ) push(0, 30);
    if (omp_get_thread_num() == 5 ) {
      sleep(3);
      steal();
      pop(10);
    }
    if (omp_get_thread_num() != 5 ) push(100, 12);
    // pop(100);
    // int_bis_element_t *e = typed_bistack_steal(int, qtype)(&queue);
    //dump(e);
    if (omp_get_thread_num() != 5 ) push(300, 30);
    //typed_queue_
    if (omp_get_thread_num() == 5 ) {
      sleep(1);
      steal();
      pop(100);
    }
  }
}

#endif
