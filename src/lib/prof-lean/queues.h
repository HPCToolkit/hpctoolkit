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

#ifndef QUEUES_H
#define QUEUES_H

//*****************************************************************************
// Description:
//
//   interface for sequential and concurrent LIFO queues (AKA stacks)
//
//*****************************************************************************



//*****************************************************************************
// macros types
//*****************************************************************************

#include "stdatomic.h"



//*****************************************************************************
// macros types
//*****************************************************************************

// routine name for a queue operation
#define queue_op(qtype, op)			\
  qtype ## _ ## op

// routine name for a typed queue operation
#define typed_queue_op(type, qtype, op)		\
  type ## _ ## qtype ## _ ## op

// init routine name for a typed queue 
#define typed_queue_init(type, qtype)		\
  typed_queue_op(type, qtype, init)			

// swap routine name for a typed queue 
#define typed_queue_swap(type, qtype)		\
  typed_queue_op(type, qtype, swap)			

// push routine name for a typed queue 
#define typed_queue_push(type, qtype)		\
  typed_queue_op(type, qtype, push)			

// pop routine name for a typed queue 
#define typed_queue_pop(type, qtype)		\
  typed_queue_op(type, qtype, pop)			

// steal routine name for a typed queue 
#define typed_queue_steal(type, qtype)		\
  typed_queue_op(type, qtype, steal)			


// define typed wrappers for a queue type 
#define typed_queue(type, qtype)				\
  void								\
  typed_queue_init(type, qtype)(type *q)			\
  {								\
    queue_op(qtype,init)((q_element_t *) q);			\
  }								\
  								\
  type *							\
  typed_queue_swap(type, qtype)(type *q, type *r)		\
  {								\
    type *e = (type *)						\
      queue_op(qtype,swap)((q_element_t *) q,			\
			   (q_element_t *) r);			\
    return e;							\
  }								\
  								\
  void								\
  typed_queue_push(type, qtype)(type *q, type *e)		\
  {								\
    queue_op(qtype,push)((q_element_t *) q,			\
			 (q_element_t *) e);			\
  }								\
  								\
  type *							\
  typed_queue_pop(type, qtype)(type *q)				\
  {								\
    type *e = (type *) queue_op(qtype,pop)((q_element_t *) q);	\
    return e;							\
  }								\
  								\
  type *							\
  typed_queue_steal(type, qtype)(type *q)			\
  {								\
    type *e = (type *)						\
      queue_op(qtype,steal)((q_element_t *) q);			\
    return e;							\
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
squeue_init
(
 q_element_t *q
);


// set q->next to point to e and return old value of q->next
q_element_t *
squeue_swap
(
  q_element_t *q,
  q_element_t *e
);


// push a singleton e or a chain beginning with e onto q 
void
squeue_push
(
  q_element_t *q,
  q_element_t *e
);


// pop a singlegon from q or return 0
q_element_t *
squeue_pop
(
  q_element_t *q
);


// steal the entire chain rooted at q 
q_element_t *
squeue_steal
(
  q_element_t *q
);



//-----------------------------------------------------------------------------
// concurrent LIFO queue interface operations
//-----------------------------------------------------------------------------

void
cqueue_init
(
 q_element_t *q
);


// set q->next to point to e and return old value of q->next
q_element_t *
cqueue_swap
(
  q_element_t *q,
  q_element_t *e
);


// push a singleton e or a chain beginning with e onto q 
void
cqueue_push
(
  q_element_t *q,
  q_element_t *e
);


// pop a singlegon from q or return 0
q_element_t *
cqueue_pop
(
  q_element_t *q
);


// steal the entire chain rooted at q 
q_element_t *
cqueue_steal
(
  q_element_t *q
);

#endif
