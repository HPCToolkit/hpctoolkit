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
// Copyright ((c)) 2002-2021, Rice University
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

#include <string.h>
#include <pthread.h>


//******************************************************************************
// macros
//******************************************************************************

#define SECONDS_UNTIL_WAKEUP 2

#define DEBUG 0

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-trace.h"
#include "gpu-trace-channel.h"
#include "gpu-trace-item.h"
#include "gpu-print.h"
#include "thread_data.h"

//******************************************************************************
// macros
//******************************************************************************

#define CHANNEL_FILL_COUNT 100


#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) gpu_trace_channel_t
#define typed_stack_elem(x) gpu_trace_item_t

// define macros that simplify use of trace channel API 
#define channel_init  \
  typed_bichannel_init(gpu_trace_item_t)

#define channel_pop   \
  typed_bichannel_pop(gpu_trace_item_t)

#define channel_push  \
  typed_bichannel_push(gpu_trace_item_t)

#define channel_reverse \
  typed_bichannel_reverse(gpu_trace_item_t)

#define channel_steal \
  typed_bichannel_steal(gpu_trace_item_t)



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct thread_data_t thread_data_t;


typedef struct gpu_trace_channel_t {
  bistack_t bistacks[2];
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  uint64_t count;
  thread_data_t *td;
} gpu_trace_channel_t;



//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for traces
typed_bichannel_impl(gpu_trace_item_t)

static void
gpu_trace_channel_signal_consumer_when_full
(
 gpu_trace_channel_t *trace_channel
)
{
  if (trace_channel->count++ > CHANNEL_FILL_COUNT) {
    trace_channel->count = 0;
    gpu_trace_channel_signal_consumer(trace_channel);
  }
}



//******************************************************************************
// interface functions
//******************************************************************************

struct thread_data_t *
gpu_trace_channel_get_td
(
 gpu_trace_channel_t *ch
)
{
  return ch->td;
}


int
gpu_trace_channel_get_stream_id
(
 gpu_trace_channel_t *ch
)
{
  return ch->td->core_profile_trace_data.id;
}


gpu_trace_channel_t *
gpu_trace_channel_alloc
(
 gpu_tag_t tag
)
{
  gpu_trace_channel_t *channel =
    hpcrun_malloc_safe(sizeof(gpu_trace_channel_t));

  memset(channel, 0, sizeof(gpu_trace_channel_t));

  channel_init(channel);

  channel->td = gpu_trace_stream_acquire(tag);

  pthread_mutex_init(&channel->mutex, NULL);
  pthread_cond_init(&channel->cond, NULL);

  return channel;
}


void
gpu_trace_channel_produce
(
 gpu_trace_channel_t *channel,
 gpu_trace_item_t *ti
)
{
  gpu_trace_item_t *cti = gpu_trace_item_alloc(channel);

  *cti = *ti;

  PRINT("\n===========TRACE_PRODUCE: ti = %p || submit = %lu, start = %lu, end = %lu, cct_node = %p\n\n",
         ti,
         ti->cpu_submit_time,
         ti->start,
         ti->end,
         ti->call_path_leaf);

  channel_push(channel, bichannel_direction_forward, cti);

  gpu_trace_channel_signal_consumer_when_full(channel);
}


void
gpu_trace_channel_consume
(
 gpu_trace_channel_t *channel
)
{

  hpcrun_set_thread_data(channel->td);

  // steal elements previously pushed by the producer
  channel_steal(channel, bichannel_direction_forward);

  // reverse them so that they are in FIFO order
  channel_reverse(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_trace_item_t *ti = channel_pop(channel, bichannel_direction_forward);
    if (!ti) break;

    PRINT("\n===========TRACE_CONSUME: ti = %p || submit = %lu, start = %lu, end = %lu, cct_node = %p\n\n",
           ti,
           ti->cpu_submit_time,
           ti->start,
           ti->end,
           ti->call_path_leaf);
    gpu_trace_item_consume(consume_one_trace_item, channel->td, ti);
    gpu_trace_item_free(channel, ti);
  }
}


void
gpu_trace_channel_await
(
 gpu_trace_channel_t *channel
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
gpu_trace_channel_signal_consumer
(
 gpu_trace_channel_t *trace_channel
)
{
  pthread_cond_signal(&trace_channel->cond);
}

