
//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-activity.h"
#include "gpu-activity-channel.h"
#include "gpu-channel-item-allocator.h"


//******************************************************************************
// macros
//******************************************************************************

#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) gpu_activity_channel_t
#define typed_stack_elem(x) gpu_activity_t

// define macros that simplify use of activity channel API 
#define channel_init  \
  typed_bichannel_init(gpu_activity_t)

#define channel_pop   \
  typed_bichannel_pop(gpu_activity_t)

#define channel_push  \
  typed_bichannel_push(gpu_activity_t)

#define channel_steal \
  typed_bichannel_steal(gpu_activity_t)

#define gpu_activity_alloc(channel)		\
  channel_item_alloc(channel, gpu_activity_t)

#define gpu_activity_free(channel, item)	\
  channel_item_free(channel, item)


//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_channel_t {
  bistack_t bistacks[2];
} gpu_activity_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_activity_channel_t *gpu_activity_channel = NULL;



//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for activities
typed_bichannel_impl(gpu_activity_t)


static gpu_activity_channel_t *
gpu_activity_channel_alloc
(
 void
)
{
  gpu_activity_channel_t *c = 
    hpcrun_malloc_safe(sizeof(gpu_activity_channel_t));

  channel_init(c);

  return c;
}



//******************************************************************************
// interface operations 
//******************************************************************************

gpu_activity_channel_t *
gpu_activity_channel_get
(
 void
)
{
  if (gpu_activity_channel == NULL) {
    gpu_activity_channel = gpu_activity_channel_alloc();
  }

  return gpu_activity_channel;
}


void
gpu_activity_channel_produce
(
 gpu_activity_channel_t *channel,
 gpu_activity_t *a
)
{
  gpu_activity_t *channel_activity = gpu_activity_alloc(channel);
  *channel_activity = *a;

  gpu_context_activity_dump(channel_activity, "PRODUCE");

  channel_push(channel, bichannel_direction_forward, channel_activity);
}


void
gpu_activity_channel_consume
(
 void
)
{
  gpu_activity_channel_t *channel = gpu_activity_channel_get();

  // steal elements previously enqueued by the producer
  channel_steal(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_activity_t *a = channel_pop(channel, bichannel_direction_forward);
    if (!a) break;
    gpu_activity_consume(a);
    gpu_activity_free(channel, a);
  }
}
