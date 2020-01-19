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

#include "stacks.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define typed_bistack_declare(type) \
  typed_bistack_functions(type, ignore)

#define typed_bistack_impl(type) \
  typed_bistack_functions(type, show)

#define bistack_op(op) \
  bistack_ ## op

// routine name for a typed bistack operation
#define typed_bistack_op(type, op) \
  type ## _bistack_ ## op

#define typed_bistack(type) \
  type ## _ ## bistack_t

#define typed_bistack_init(type) \
  typed_bistack_op(type, init)

#define typed_bistack_push(type) \
  typed_bistack_op(type, push)

#define typed_bistack_pop(type) \
  typed_bistack_op(type, pop)

#define typed_bistack_reverse(type) \
  typed_bistack_op(type, reverse)

#define typed_bistack_steal(type) \
  typed_bistack_op(type, steal)

// define typed wrappers for a bistack type
#define typed_bistack_functions(type, macro) \
\
  void \
  typed_bistack_init(type) \
  (typed_bistack(type) *s) \
  macro({ \
    bistack_op(init) ((bistack_t *) s); \
  }) \
\
  void \
  typed_bistack_push(type) \
  (typed_bistack(type) *s, typed_stack_elem(type) *e) \
  macro({ \
    bistack_op(push) ((bistack_t *) s, \
    (s_element_t *) e); \
  }) \
\
  typed_stack_elem(type) * \
  typed_bistack_pop(type) \
  (typed_bistack(type) *s) \
  macro({ \
    typed_stack_elem(type) *e = (typed_stack_elem(type) *) \
    bistack_op(pop) ((bistack_t *) s); \
    return e; \
  }) \
\
  void \
  typed_bistack_reverse(type) \
  (typed_bistack(type) *s) \
  macro({ \
    bistack_op(reverse) ((bistack_t *) s); \
  }) \
\
  void \
  typed_bistack_steal(type) \
  (typed_bistack(type) *s) \
  macro({ \
    bistack_op(steal) ((bistack_t *) s); \
  })



//*****************************************************************************
// types
//*****************************************************************************

typedef struct bistack_s {
  s_element_ptr_t produced;
  s_element_ptr_t to_consume;
} bistack_t;


//*****************************************************************************
// interface functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// sequential LIFO bistack interface operations
//-----------------------------------------------------------------------------

void
bistack_init
(
 bistack_t *s
);


// push a singleton e or a chain beginning with e onto the produced stack
void
bistack_push
(
 bistack_t *s,
 s_element_t *e
);


// pop a singleton from s from the to_consume stack or return 0
s_element_t *
bistack_pop
(
 bistack_t *s
);


// reverse the order of the to_consume stack
void
bistack_reverse
(
 bistack_t *s
);


// initialize the to_consume stack with the produced stack
void
bistack_steal
(
 bistack_t *s
);



#endif
