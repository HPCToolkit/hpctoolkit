// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

//******************************************************************************
// system includes
//******************************************************************************

#include <pthread.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-channel-item-allocator.h"
#include "gpu-operation-channel.h"
#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"

#define DEBUG 0
#include "gpu-print.h"


//******************************************************************************
// macros
//******************************************************************************



#define CHANNEL_FILL_COUNT 100


#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) gpu_operation_channel_t
#define typed_stack_elem(x) gpu_operation_item_t

// define macros that simplify use of operation channel API
#define channel_init  \
  typed_bichannel_init(gpu_operation_item_t)

#define channel_pop   \
  typed_bichannel_pop(gpu_operation_item_t)

#define channel_push  \
  typed_bichannel_push(gpu_operation_item_t)

#define channel_reverse \
  typed_bichannel_reverse(gpu_operation_item_t)

#define channel_steal \
  typed_bichannel_steal(gpu_operation_item_t)


#define SECONDS_UNTIL_WAKEUP 1



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_operation_channel_t {
  bistack_t bistacks[2];
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  uint64_t count;
} gpu_operation_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_operation_channel_t *gpu_operation_channel = NULL;


//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for activities
typed_bichannel_impl(gpu_operation_item_t)


static void
gpu_operation_channel_signal_consumer_when_full
(
 gpu_operation_channel_t *channel
)
{
  if (channel->count++ > CHANNEL_FILL_COUNT) {
    channel->count = 0;
    gpu_operation_channel_signal_consumer(channel);
  }
}


static gpu_operation_channel_t *
gpu_operation_channel_alloc
(
void
)
{
  gpu_operation_channel_t *channel = hpcrun_malloc_safe(sizeof(gpu_operation_channel_t));

  memset(channel, 0, sizeof(gpu_operation_channel_t));

  channel_init(channel);


  pthread_mutex_init(&channel->mutex, NULL);
  pthread_cond_init(&channel->cond, NULL);

  return channel;
}



//******************************************************************************
// interface operations 
//******************************************************************************


gpu_operation_channel_t *
gpu_operation_channel_get
(
 void
)
{
  if (gpu_operation_channel == NULL) {
    gpu_operation_channel = gpu_operation_channel_alloc();
  }

  return gpu_operation_channel;
}


void
gpu_operation_channel_produce
(
 gpu_operation_channel_t *channel,
 gpu_operation_item_t *it
)
{
  gpu_operation_item_t *new_item = gpu_operation_item_alloc(channel);
  *new_item = *it;

  PRINT("\nOPERATION_PRODUCE: channel = %p || return_channel = %p -> activity = %p | corr = %u kind = %s, type = %s\n\n",
         channel, new_item->channel, &new_item->activity,
         (new_item->activity.kind == GPU_ACTIVITY_MEMCPY)?new_item->activity.details.memcpy.correlation_id:new_item->activity.details.kernel.correlation_id,
         gpu_kind_to_string(new_item->activity.kind),
         gpu_type_to_string(new_item->activity.details.memcpy.copyKind));

  channel_push(channel, bichannel_direction_forward, new_item);

  gpu_operation_channel_signal_consumer_when_full(channel);

}


void
gpu_operation_channel_consume
(
 gpu_operation_channel_t *channel
)
{

  // steal elements previously pushed by the producer
  channel_steal(channel, bichannel_direction_forward);

  // reverse them so that they are in FIFO order
  channel_reverse(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_operation_item_t *it = channel_pop(channel, bichannel_direction_forward);

    if (!it) break;

    PRINT("\nOPERATION_CONSUME: op_channel = %p || channel = %p , activity = %p | corr = %u, kind = %s, type = %s\n",
           channel, it->channel, &it->activity,
           (it->activity.kind == GPU_ACTIVITY_MEMCPY)?it->activity.details.memcpy.correlation_id:it->activity.details.kernel.correlation_id,
           gpu_kind_to_string(it->activity.kind),
           gpu_type_to_string(it->activity.details.memcpy.copyKind));

    gpu_operation_item_consume(gpu_operation_item_process, it);
    gpu_operation_item_free(channel, it);
  }
}


void
gpu_operation_channel_await
(
gpu_operation_channel_t *channel
)
{
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time); // get current time
  time.tv_sec += SECONDS_UNTIL_WAKEUP;

  // wait for a signal or for a few seconds. periodically waking
  // up avoids missing a signal.
  pthread_cond_timedwait(&channel->cond, &channel->mutex, &time);
}


void
gpu_operation_channel_signal_consumer
(
gpu_operation_channel_t *channel
)
{
  pthread_cond_signal(&channel->cond);
}
