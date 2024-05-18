// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
