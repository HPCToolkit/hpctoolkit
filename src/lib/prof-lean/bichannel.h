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

#ifndef bichannel_h
#define bichannel_h



//*****************************************************************************
// local includes
//*****************************************************************************

#include "bistack.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define bichannel_macro_body_ignore(x) ;
#define bichannel_macro_body_show(x) x

#define typed_bichannel_declare(type) \
  typed_bichannel_functions(type, bichannel_macro_body_ignore)

#define typed_bichannel_impl(type) \
  typed_bichannel_functions(type, bichannel_macro_body_show)

#define bichannel_op(op) \
  bichannel_ ## op

// synthesize the name of a typed channel

#define typed_bichannel(type) \
  type ## _bichannel_t

// synthesize name for an operation on a typed channel

#define typed_bichannel_op(type, op) \
  type ## _bichannel_ ## op

#define typed_bichannel_init(type) \
  typed_bichannel_op(type, init)

#define typed_bichannel_push(type) \
  typed_bichannel_op(type, push)

#define typed_bichannel_pop(type) \
  typed_bichannel_op(type, pop)

#define typed_bichannel_reverse(type) \
  typed_bichannel_op(type, reverse)

#define typed_bichannel_steal(type) \
  typed_bichannel_op(type, steal)


// define typed wrappers for a unordered_stack type
#define typed_bichannel_functions(type, macro) \
\
  void \
  typed_bichannel_init(type) \
  (typed_bichannel(type) *c) \
  macro({ \
    bichannel_op(init) ((bichannel_t *) c); \
  }) \
\
  void \
  typed_bichannel_push(type) \
  (typed_bichannel(type) *c, bichannel_direction_t dir, \
                              typed_stack_elem(type) *e) \
  macro({ \
    bichannel_op(push) ((bichannel_t *) c, dir, \
    (s_element_t *) e); \
  }) \
\
  typed_stack_elem(type) * \
  typed_bichannel_pop(type) \
  (typed_bichannel(type) *c, bichannel_direction_t dir) \
  macro({ \
    typed_stack_elem(type) *e = (typed_stack_elem(type) *) \
    bichannel_op(pop) ((bichannel_t *) c, dir); \
    return e; \
  }) \
\
  void \
  typed_bichannel_reverse(type) \
  (typed_bichannel(type) *c, bichannel_direction_t dir) \
  macro({ \
    bichannel_op(reverse) ((bichannel_t *) c, dir); \
  }) \
\
  void \
  typed_bichannel_steal(type) \
  (typed_bichannel(type) *c, bichannel_direction_t dir) \
  macro({ \
    bichannel_op(steal) ((bichannel_t *) c, dir); \
  })



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef enum {
 bichannel_direction_forward = 0,
 bichannel_direction_backward = 1
} bichannel_direction_t;


typedef struct bichannel_t {
  bistack_t bistacks[2];
} bichannel_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

void 
bichannel_init
(
 bichannel_t *ch
);


void
bichannel_push
(
 bichannel_t *ch, 
 bichannel_direction_t dir, 
 s_element_t *e
);


s_element_t *
bichannel_pop
(
 bichannel_t *ch, 
 bichannel_direction_t dir
);


void
bichannel_reverse
(
 bichannel_t *ch, 
 bichannel_direction_t dir
);


void
bichannel_steal
(
 bichannel_t *ch, 
 bichannel_direction_t dir
);



#endif
