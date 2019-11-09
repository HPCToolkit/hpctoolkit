//******************************************************************************
// local includes
//******************************************************************************

//******************************************************************************
// local includes
//******************************************************************************


// #include <lib/prof-lean/bichannel.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-channel.h"
#include "gpu-correlation.h"
#include "gpu-correlation-channel.h"
#include "gpu-correlation-channel-set.h"



//******************************************************************************
// macros
//******************************************************************************

#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) gpu_correlation_channel_t
#define typed_stack_elem(x) gpu_correlation_t

// define macros that simplify use of correlation channel API 
#define channel_init  \
  typed_bichannel_init(gpu_correlation_t)

#define channel_pop   \
  typed_bichannel_pop(gpu_correlation_t)

#define channel_push  \
  typed_bichannel_push(gpu_correlation_t)

#define channel_steal \
  typed_bichannel_steal(gpu_correlation_t)



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t {
  bistack_t bistacks[2];
} gpu_correlation_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_correlation_channel_t *gpu_correlation_channel = NULL;



//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for correlations
typed_bichannel_impl(gpu_correlation_t)


static gpu_correlation_channel_t *
gpu_correlation_channel_alloc
(
 void
)
{
  gpu_correlation_channel_t *c = 
    hpcrun_malloc_safe(sizeof(gpu_correlation_channel_t));

  channel_init(c);

  gpu_correlation_channel_set_insert(c);

  return c;
}


static gpu_correlation_channel_t *
gpu_correlation_channel_get
(
 void
)
{
  if (gpu_correlation_channel == NULL) {
    gpu_correlation_channel = gpu_correlation_channel_alloc();
  }

  return gpu_correlation_channel;
}



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_correlation_channel_produce
(
 uint64_t host_op_id,
 cct_node_t *api_node,
 cct_node_t *func_node
)
{
  gpu_correlation_channel_t *corr_channel = gpu_correlation_channel_get();
  gpu_activity_channel_t *activity_channel = gpu_activity_channel_get();

  gpu_correlation_t *c = gpu_correlation_alloc(corr_channel);

  gpu_correlation_produce(c, host_op_id, api_node, func_node,
			  activity_channel);

  channel_push(corr_channel, bichannel_direction_forward, c);
}


void
gpu_correlation_channel_consume
(
 gpu_correlation_channel_t *channel
)
{
  // steal elements previously enqueued by the producer
  channel_steal(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_correlation_t *c = channel_pop(channel, bichannel_direction_forward);
    if (!c) break;
    gpu_correlation_consume(c);
    gpu_correlation_free(channel, c);
  }
}


//******************************************************************************
// unit testing
//******************************************************************************


#define UNIT_TEST 1

#if UNIT_TEST

#include <stdlib.h>
#include "gpu-correlation-channel-set.h"


void *hpcrun_malloc_safe
(
 size_t s
) 
{
  return malloc(s);
}


gpu_activity_channel_t *
gpu_activity_channel_get
(
 void
) 
{
  return (gpu_activity_channel_t *) 0x5000;
}


int
main
(
 int argc, 
 char **argv
)
{
  int i;
  for(i = 0; i < 10; i++) {
    cct_node_t *api = (cct_node_t *) ((long) i + 100);
    cct_node_t *func = (cct_node_t *) ((long) i + 200);
    gpu_correlation_channel_produce(i, api, func);
  }
  gpu_correlation_channel_set_consume();
  for(i = 20; i < 30; i++) {
    cct_node_t *api = (cct_node_t *) ((long) i + 100);
    cct_node_t *func = (cct_node_t *) ((long) i + 200);
    gpu_correlation_channel_produce(i, api, func);
  }
  gpu_correlation_channel_set_consume();
}

#endif
