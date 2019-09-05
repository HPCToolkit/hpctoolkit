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

#include "unordered_stack.h"


//*****************************************************************************
// interface functions
//*****************************************************************************


#define Ad(u_s) u_s.aptr
#define Ap(u_s) u_s->aptr

void unordered_stack_init
(
    u_stack_t *p
)
{
    atomic_init(&Ad(p->produced), 0);
    atomic_init(&Ad(p->to_consume), 0);
}

void
unordered_stack_push
(
    u_stack_t *p,
    s_element_t *e
)
{
    cstack_push(&p->produced, e);
}

s_element_t *
unordered_stack_pop
(
    u_stack_t *p
)
{
    //for consumer we can use memory order relaxed
    s_element_t *e = sstack_pop(&p->to_consume);
    return e;
}

void
unordered_stack_steal
(
    u_stack_t *p
)
{
    s_element_t *s = cstack_steal(&p->produced);
    atomic_store_explicit(&Ad(p->to_consume), s, memory_order_relaxed);
}

//*****************************************************************************
// unit test
//*****************************************************************************

#define UNIT_TEST 1
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
typedef u_stack_t typed_unordered_stack(int);

//typed_queue_elem_ptr(int) queue;
typed_unordered_stack(int) pair;

typed_unordered_stack_impl(int)

typed_stack_elem(int) *
typed_stack_elem_fn(int,new)(int value)
{
    typed_stack_elem(int) *e =(typed_stack_elem(int)* ) malloc(sizeof(int_s_element_t));
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
        typed_stack_elem(int) *e = typed_unordered_stack_pop(int)(&pair);
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
        typed_unordered_stack_push(int)(&pair, typed_stack_elem_fn(int, new)(i));
    }
}

void
steal
        (

                )
{
    typed_unordered_stack_steal(int)(&pair);
}


/*void
dump
        (
                int_s_element_t *e
        )
{
    int i;
    for(; e; e = (int_s_element_t *) typed_stack_elem_ptr_get(int,cstack)(&e->next)) {
        printf("%d stole %d\n", omp_get_thread_num(), e->value);
    }
}*/


int
main
        (
                int argc,
                char **argv
        )
{
    unordered_stack_init(&pair);
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
       // int_u_s_element_t *e = typed_unordered_stack_steal(int, qtype)(&queue);
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