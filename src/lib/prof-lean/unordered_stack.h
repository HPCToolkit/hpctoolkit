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

#ifndef UNORDERED_STACK_H
#define UNORDERED_STACK_H

//*****************************************************************************
// Description:
//
//
//
//*****************************************************************************



//*****************************************************************************
// local includes
//*****************************************************************************

#include "stdatomic.h"
#include "stacks.h"


//*****************************************************************************
// macros
//*****************************************************************************

#define typed_unordered_stack_declare(type) \
typed_unordered_stack_functions(type, ignore)

#define typed_unordered_stack_impl(type) \
typed_unordered_stack_functions(type, show)

#define unordered_stack_op(op)			\
  unordered_stack_ ## op

// routine name for a typed unordered_stack operation
#define typed_unordered_stack_op(type, op)		\
   type ## _unordered_stack_ ## op

#define typed_unordered_stack(type)  \
  type ## _ ## u_stack_t

#define typed_unordered_stack_init(type) \
    typed_unordered_stack_op(type, init)

#define typed_unordered_stack_push(type)		\
  typed_unordered_stack_op(type, push)

#define typed_unordered_stack_pop(type)		\
  typed_unordered_stack_op(type, pop)

#define typed_unordered_stack_steal(type) \
  typed_unordered_stack_op(type, steal)


// define typed wrappers for a unordered_stack type
#define typed_unordered_stack_functions(type, macro)				\
\
\
  void                        \
  typed_unordered_stack_init(type)   \
    (typed_unordered_stack(type) *p)   \
    macro({ \
        unordered_stack_op(init) ((u_stack_t *) p); \
    }) \
  								\
 \
  void								\
  typed_unordered_stack_push(type)					\
    (typed_unordered_stack(type) *q, typed_stack_elem(type) *e)	\
  macro({								\
        unordered_stack_op(push) ((u_stack_t *) q,			\
			 (s_element_t *) e);			\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_unordered_stack_pop(type)					\
  (typed_unordered_stack(type) *q)				\
  macro({								\
    typed_stack_elem(type) *e = (typed_stack_elem(type) *)	\
      unordered_stack_op(pop) ((u_stack_t *) q);		\
    return e;							\
  }) \
  \
   void								\
  typed_unordered_stack_steal(type)					\
    (typed_unordered_stack(type) *q)	\
  macro({								\
        unordered_stack_op(steal) ((u_stack_t *) q);			\
  })								\




//*****************************************************************************
// types
//*****************************************************************************

typedef struct u_stack_s {
    s_element_ptr_t produced;
    s_element_ptr_t to_consume;
} u_stack_t;


//*****************************************************************************
// interface functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// sequential LIFO unordered_stack interface operations
//-----------------------------------------------------------------------------

void
unordered_stack_init
(
    u_stack_t *p
);

// push a singleton e or a chain beginning with e onto q
void
unordered_stack_push
(
 u_stack_t *p,
 s_element_t *e
);

// pop a singlegon from q or return 0
s_element_t *
unordered_stack_pop
(
 u_stack_t *p
);

void
unordered_stack_steal
(
 u_stack_t *p
);



#endif
