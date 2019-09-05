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

#ifndef STACK_H
#define STACK_H

//*****************************************************************************
// Description:
//
//   interface for sequential and concurrent LIFO stacks (AKA stacks)
//
//*****************************************************************************



//*****************************************************************************
// local includes
//*****************************************************************************

#include "stdatomic.h"



//*****************************************************************************
// macros 
//*****************************************************************************

#define ignore(x) ;
#define show(x) x

#define typed_stack_declare(type) \
typed_stack(type, ignore)

#define typed_stack_impl(type) \
typed_stack(type, show)

// routine name for a stack operation
#define stack_op(qtype, op)			\
  qtype ## _ ## op

// typed stack pointer
#define typed_stack_elem_ptr(type)		\
  type ## _ ## s_element_ptr_t

#define typed_stack_elem(type)			\
  type ## _ ## s_element_t

#define typed_stack_elem_fn(type, fn)		\
  type ## _ ## s_element_ ## fn

// routine name for a typed stack operation
#define typed_stack_op(type, qtype, op)		\
  type ## _ ## qtype ## _ ## op

// ptr set routine name for a typed stack 
#define typed_stack_elem_ptr_set(type, qtype)	\
  typed_stack_op(type, qtype, ptr_set)

// ptr get routine name for a typed stack 
#define typed_stack_elem_ptr_get(type, qtype)	\
  typed_stack_op(type, qtype, ptr_get)

// swap routine name for a typed stack 
#define typed_stack_swap(type, qtype)		\
  typed_stack_op(type, qtype, swap)

// push routine name for a typed stack 
#define typed_stack_push(type, qtype)		\
  typed_stack_op(type, qtype, push)

// pop routine name for a typed stack 
#define typed_stack_pop(type, qtype)		\
  typed_stack_op(type, qtype, pop)

// steal routine name for a typed stack 
#define typed_stack_steal(type, qtype)		\
  typed_stack_op(type, qtype, steal)


// define typed wrappers for a stack type 
#define typed_stack(type, qtype, macro)				\
  void								\
  typed_stack_elem_ptr_set(type, qtype)				\
    (typed_stack_elem_ptr(type) *e,				\
     typed_stack_elem(type) *v)					\
  macro({								\
    stack_op(qtype,ptr_set)((s_element_ptr_t *) e,		\
			    (s_element_t *) v);			\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_stack_elem_ptr_get(type, qtype)				\
    (typed_stack_elem_ptr(type) *e)				\
  macro({								\
    typed_stack_elem(type) *r =	(typed_stack_elem(type) *)	\
      stack_op(qtype,ptr_get)((s_element_ptr_t *) e);		\
    return r;							\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_stack_swap(type, qtype)					\
    (typed_stack_elem_ptr(type) *q, typed_stack_elem(type) *v)	\
  macro({								\
    typed_stack_elem(type) *e = (typed_stack_elem(type) *)	\
      stack_op(qtype,swap)((s_element_ptr_t *) q,		\
			   (s_element_t *) v);			\
    return e;							\
  })							\
  								\
  void								\
  typed_stack_push(type, qtype)					\
    (typed_stack_elem_ptr(type) *q, typed_stack_elem(type) *e)	\
  macro({								\
    stack_op(qtype,push)((s_element_ptr_t *) q,			\
			 (s_element_t *) e);			\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_stack_pop(type, qtype)					\
  (typed_stack_elem_ptr(type) *q)				\
  macro({								\
    typed_stack_elem(type) *e = (typed_stack_elem(type) *)	\
      stack_op(qtype,pop)((s_element_ptr_t *) q);		\
    return e;							\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_stack_steal(type, qtype)				\
  (typed_stack_elem_ptr(type) *q)				\
  macro({								\
    typed_stack_elem(type) *e = (typed_stack_elem(type) *)	\
      stack_op(qtype,steal)((s_element_ptr_t *) q);		\
    return e;							\
  })


//*****************************************************************************
// types
//*****************************************************************************

typedef struct s_element_ptr_u {
    _Atomic(struct s_element_s*) aptr;
} s_element_ptr_t;


typedef struct s_element_s {
    s_element_ptr_t next;
} s_element_t;



//*****************************************************************************
// interface functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// sequential LIFO stack interface operations
//-----------------------------------------------------------------------------

void
sstack_ptr_set
        (
                s_element_ptr_t *e,
                s_element_t *v
        );


s_element_t *
sstack_ptr_get
        (
                s_element_ptr_t *e
        );


// set q->next to point to e and return old value of q->next
s_element_t *
sstack_swap
        (
                s_element_ptr_t *q,
                s_element_t *e
        );


// push a singleton e or a chain beginning with e onto q 
void
sstack_push
        (
                s_element_ptr_t *q,
                s_element_t *e
        );


// pop a singlegon from q or return 0
s_element_t *
sstack_pop
        (
                s_element_ptr_t *q
        );


// steal the entire chain rooted at q 
s_element_t *
sstack_steal
        (
                s_element_ptr_t *q
        );



//-----------------------------------------------------------------------------
// concurrent LIFO stack interface operations
//-----------------------------------------------------------------------------

void
cstack_ptr_set
        (
                s_element_ptr_t *e,
                s_element_t *v
        );


s_element_t *
cstack_ptr_get
        (
                s_element_ptr_t *e
        );


// set q->next to point to e and return old value of q->next
s_element_t *
cstack_swap
        (
                s_element_ptr_t *q,
                s_element_t *e
        );


// push a singleton e or a chain beginning with e onto q 
void
cstack_push
        (
                s_element_ptr_t *q,
                s_element_t *e
        );


// pop a singlegon from q or return 0
s_element_t *
cstack_pop
        (
                s_element_ptr_t *q
        );


// steal the entire chain rooted at q 
s_element_t *
cstack_steal
        (
                s_element_ptr_t *q
        );

#endif

