
//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stacks.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-correlation-channel.h"
#include "gpu-correlation-channel-set.h"



//******************************************************************************
// macros
//******************************************************************************

#define channel_stack_push  \
  typed_stack_push(gpu_correlation_channel_ptr_t, cstack)

#define channel_stack_forall \
  typed_stack_forall(gpu_correlation_channel_ptr_t, cstack)

#define channel_stack_elem_t \
  typed_stack_elem(gpu_correlation_channel_ptr_t)

#define channel_stack_elem_ptr_set \
  typed_stack_elem_ptr_set(gpu_correlation_channel_ptr_t, cstack)



//******************************************************************************
// type declarations
//******************************************************************************

//----------------------------------------------------------
// support for a stack of correlation channels
//----------------------------------------------------------

typedef gpu_correlation_channel_t *gpu_correlation_channel_ptr_t;


typedef struct {
  s_element_ptr_t next;
  gpu_correlation_channel_ptr_t channel;
} typed_stack_elem(gpu_correlation_channel_ptr_t);


typed_stack_declare_type(gpu_correlation_channel_ptr_t);



//******************************************************************************
// local data
//******************************************************************************

static 
typed_stack_elem_ptr(gpu_correlation_channel_ptr_t) 
gpu_correlation_channel_stack;



//******************************************************************************
// private operations
//******************************************************************************

// implement stack of correlation channels
typed_stack_impl(gpu_correlation_channel_ptr_t, cstack);


static void
channel_forone
(
 channel_stack_elem_t *se,
 void *arg
)
{
  gpu_correlation_channel_t *channel = se->channel;

  gpu_correlation_channel_fn_t channel_fn =
    (gpu_correlation_channel_fn_t) arg;

  channel_fn(channel);
}


static void
gpu_correlation_channel_set_forall
(
 gpu_correlation_channel_fn_t channel_fn
)
{
  channel_stack_forall(&gpu_correlation_channel_stack, channel_forone, 
		       channel_fn);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_correlation_channel_set_insert
(
 gpu_correlation_channel_t *channel
)
{
  // allocate and initialize new entry for channel stack
  channel_stack_elem_t *e = 
    (channel_stack_elem_t *) hpcrun_malloc_safe(sizeof(channel_stack_elem_t));

  // initialize the new entry
  e->channel = channel;
  channel_stack_elem_ptr_set(e, 0); // clear the entry's next ptr

  // add the entry to the channel stack
  channel_stack_push(&gpu_correlation_channel_stack, e);
}


void
gpu_correlation_channel_set_consume
(
 void
)
{
  gpu_correlation_channel_set_forall(gpu_correlation_channel_consume);
}
