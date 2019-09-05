//
// Created by ax4 on 7/29/19.
//

#ifndef HPCTOOLKIT_BI_UNORDERED_CHANNEL_H
#define HPCTOOLKIT_BI_UNORDERED_CHANNEL_H

#include "unordered_stack.h"
#include "stacks.h"


#define typed_bi_unordered_channel_declare(type) \
typed_bi_unordered_channel_functions(type, ignore)

#define typed_bi_unordered_channel_impl(type) \
typed_bi_unordered_channel_functions(type, show)

#define bi_unordered_channel_op(op)			\
  bi_unordered_channel_ ## op

// routine name for a typed unordered_stack operation
#define typed_bi_unordered_channel_op(type, op)		\
   type ## _bi_unordered_channel_ ## op

#define typed_bi_unordered_channel(type)  \
  type ## _ ## bi_unordered_channel_t

#define typed_bi_unordered_channel_init(type) \
    typed_bi_unordered_channel_op(type, init)

#define typed_bi_unordered_channel_push(type)		\
  typed_bi_unordered_channel_op(type, push)

#define typed_bi_unordered_channel_pop(type)		\
  typed_bi_unordered_channel_op(type, pop)

#define typed_bi_unordered_channel_steal(type) \
  typed_bi_unordered_channel_op(type, steal)


// define typed wrappers for a unordered_stack type
#define typed_bi_unordered_channel_functions(type, macro)				\
\
\
  void                        \
  typed_bi_unordered_channel_init(type)   \
    (typed_bi_unordered_channel(type) p)   \
    macro({ \
        bi_unordered_channel_op(init) ((bi_unordered_channel_t) p); \
    }) \
  								\
 \
  void								\
  typed_bi_unordered_channel_push(type)					\
    (typed_bi_unordered_channel(type) *q, channel_direction_t dir, typed_stack_elem(type) *e)	\
  macro({								\
        bi_unordered_channel_op(push) ((bi_unordered_channel_t *) q, dir, 			\
			 (s_element_t *) e);			\
  })								\
  								\
  typed_stack_elem(type) *					\
  typed_bi_unordered_channel_pop(type)					\
  (typed_bi_unordered_channel(type) *q, channel_direction_t dir)				\
  macro({								\
    typed_stack_elem(type) *e = (typed_stack_elem(type) *)	\
      bi_unordered_channel_op(pop) ((bi_unordered_channel_t *) q, dir);		\
    return e;							\
  }) \
  \
  void								\
  typed_bi_unordered_channel_steal(type)					\
    (typed_bi_unordered_channel(type) *q, channel_direction_t dir)	\
  macro({								\
        bi_unordered_channel_op(steal) ((bi_unordered_channel_t *) q, dir);			\
  })								\


typedef enum {
    channel_direction_forward,
    channel_direction_backward
} channel_direction_t;

typedef struct bi_unordered_channel_s {
    u_stack_t forward_stack;
    u_stack_t backward_stack;
} bi_unordered_channel_t;


void bi_unordered_channel_push(bi_unordered_channel_t *ch, channel_direction_t dir, s_element_t *e);
s_element_t * bi_unordered_channel_pop(bi_unordered_channel_t *ch, channel_direction_t dir);
void bi_unordered_channel_steal(bi_unordered_channel_t *ch, channel_direction_t dir);
void bi_unordered_channel_init(bi_unordered_channel_t ch);


#endif //HPCTOOLKIT_BI_UNORDERED_CHANNEL_H
