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

#ifndef stacks_h
#define stacks_h

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

#define stacks_macro_body_ignore(x) ;
#define stacks_macro_body_show(x) x

#define typed_stack_declare_type(type)		\
  typedef typed_stack_elem(type) * typed_stack_elem_ptr(type)

#define typed_stack_declare(type, flavor)		\
  typed_stack(type, flavor, stacks_macro_body_ignore)

#define typed_stack_impl(type, flavor)			\
  typed_stack(type, flavor, stacks_macro_body_show)

// routine name for a stack operation
#define stack_op(qtype, op) \
  qtype ## _ ## op

// typed stack pointer
#define typed_stack_elem_ptr(type) \
  type ## _ ## s_element_ptr_t

#define typed_stack_elem(type) \
  type ## _ ## s_element_t

#define typed_stack_elem_fn(type, fn) \
  type ## _ ## s_element_ ## fn

// routine name for a typed stack operation
#define typed_stack_op(type, qtype, op) \
  type ## _ ## qtype ## _ ## op

// ptr set routine name for a typed stack 
#define typed_stack_elem_ptr_set(type, qtype) \
  typed_stack_op(type, qtype, ptr_set)

// ptr get routine name for a typed stack 
#define typed_stack_elem_ptr_get(type, qtype) \
  typed_stack_op(type, qtype, ptr_get)

// swap routine name for a typed stack 
#define typed_stack_swap(type, qtype) \
  typed_stack_op(type, qtype, swap)

// push routine name for a typed stack 
#define typed_stack_push(type, qtype) \
  typed_stack_op(type, qtype, push)

// pop routine name for a typed stack 
#define typed_stack_pop(type, qtype) \
  typed_stack_op(type, qtype, pop)

// steal routine name for a typed stack 
#define typed_stack_steal(type, qtype) \
  typed_stack_op(type, qtype, steal)

// forall routine name for a typed stack 
#define typed_stack_forall(type, qtype) \
  typed_stack_op(type, qtype, forall)


// define typed wrappers for a stack type 
#define typed_stack(type, qtype, macro) \
  void \
  typed_stack_elem_ptr_set(type, qtype) \
  (typed_stack_elem_ptr(type) e, typed_stack_elem(type) *v) \
  macro({ \
    stack_op(qtype,ptr_set)((s_element_ptr_t *) e, \
      (s_element_t *) v); \
  }) \
\
  typed_stack_elem(type) * \
  typed_stack_elem_ptr_get(type, qtype) \
  (typed_stack_elem_ptr(type) *e) \
  macro({ \
    typed_stack_elem(type) *r = (typed_stack_elem(type) *) \
    stack_op(qtype,ptr_get)((s_element_ptr_t *) e); \
    return r; \
  }) \
\
  typed_stack_elem(type) * \
  typed_stack_swap(type, qtype) \
  (typed_stack_elem_ptr(type) *s, typed_stack_elem(type) *v) \
  macro({ \
    typed_stack_elem(type) *e = (typed_stack_elem(type) *) \
    stack_op(qtype,swap)((s_element_ptr_t *) s, \
      (s_element_t *) v); \
    return e; \
  }) \
\
  void \
  typed_stack_push(type, qtype) \
  (typed_stack_elem_ptr(type) *s, typed_stack_elem(type) *e) \
  macro({ \
    stack_op(qtype,push)((s_element_ptr_t *) s, \
    (s_element_t *) e); \
  }) \
\
  typed_stack_elem(type) * \
  typed_stack_pop(type, qtype) \
  (typed_stack_elem_ptr(type) *s) \
  macro({ \
    typed_stack_elem(type) *e = (typed_stack_elem(type) *) \
    stack_op(qtype,pop)((s_element_ptr_t *) s); \
    return e; \
  }) \
\
  typed_stack_elem(type) * \
  typed_stack_steal(type, qtype) \
  (typed_stack_elem_ptr(type) *s) \
  macro({ \
    typed_stack_elem(type) *e = (typed_stack_elem(type) *) \
    stack_op(qtype,steal)((s_element_ptr_t *) s); \
    return e; \
  }) \
\
 void typed_stack_forall(type, qtype) \
 (typed_stack_elem_ptr(type) *s, \
  void (*fn)(typed_stack_elem(type) *, void *), void *arg) \
  macro({ \
    stack_op(qtype,forall)((s_element_ptr_t *) s, (stack_forall_fn_t) fn, arg);	\
  })



//*****************************************************************************
// types
//*****************************************************************************

typedef struct s_element_ptr_t {
  _Atomic(struct s_element_ptr_t *) aptr;
} s_element_ptr_t;


typedef struct s_element_t {
  s_element_ptr_t next;
} s_element_t;


typedef void (*stack_forall_fn_t)(s_element_t *e, void *arg);



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


// set s->next to point to e and return old value of s->next
s_element_t *
sstack_swap
(
 s_element_ptr_t *s,
 s_element_t *e
);


// push a singleton e or a chain beginning with e onto s
void
sstack_push
(
 s_element_ptr_t *s,
 s_element_t *e
);


// pop a singlegon from s or return 0
s_element_t *
sstack_pop
(
 s_element_ptr_t *s
);


// steal the entire chain rooted at s 
s_element_t *
sstack_steal
(
 s_element_ptr_t *s
);


// reverse the entire chain rooted at s and set s to be the previous tail
void
sstack_reverse
(
 s_element_ptr_t *s
);


// iterate over each element e in the stack, call fn(e, arg) 
void
sstack_forall
(
 s_element_ptr_t *s,
 stack_forall_fn_t fn,
 void *arg
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


// set s->next to point to e and return old value of s->next
s_element_t *
cstack_swap
(
 s_element_ptr_t *s,
 s_element_t *e
);


// push a singleton e or a chain beginning with e onto s 
void
cstack_push
(
 s_element_ptr_t *s,
 s_element_t *e
);


// pop a singlegon from s or return 0
s_element_t *
cstack_pop
(
 s_element_ptr_t *s
);


// steal the entire chain rooted at s 
s_element_t *
cstack_steal
(
 s_element_ptr_t *s
);


// iterate over each element e in the stack, call fn(e, arg) 
// note: unsafe to use concurrently with cstack_steal or cstack_pop 
void
cstack_forall
(
 s_element_ptr_t *s,
 stack_forall_fn_t fn,
 void *arg
);



#endif

