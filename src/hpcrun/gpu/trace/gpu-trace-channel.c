// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../common/lean/collections/mpsc-queue-entry-data.h"

#include "../../thread_data.h"
#include "../../memory/hpcrun-malloc.h"

#include "gpu-trace-channel.h"
#include "gpu-trace-item.h"



//******************************************************************************
// generic code - macro expansions
//******************************************************************************

typedef struct mpscq_entry_t {
  MPSC_QUEUE_ENTRY_DATA(struct mpscq_entry_t);
  gpu_trace_item_t trace_item;
} mpscq_entry_t;


#define MPSC_QUEUE_PREFIX         mpscq
#define MPSC_QUEUE_ENTRY_TYPE     mpscq_entry_t
#define MPSC_QUEUE_ALLOCATE       hpcrun_malloc_safe
#include "../../../common/lean/collections/mpsc-queue.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_channel_t {
  mpscq_t mpsc_queue;
  thread_data_t *thread_data;

  void (*on_send_fn)(void *);
  void *on_send_arg;

  void (*on_receive_fn)(void *);
  void *on_receive_arg;
} gpu_trace_channel_t;



//******************************************************************************
// private functions
//******************************************************************************

static mpscq_allocator_t *
get_local_allocator
(
 void
)
{
  static __thread mpscq_allocator_t *allocator = NULL;

  if (allocator == NULL) {
    allocator = hpcrun_malloc_safe(sizeof(mpscq_allocator_t));
    mpscq_init_allocator(allocator);
  }

  return allocator;
}



//******************************************************************************
// interface functions
//******************************************************************************

thread_data_t *
gpu_trace_channel_get_thread_data
(
 gpu_trace_channel_t *channel
)
{
  return channel->thread_data;
}


gpu_trace_channel_t *
gpu_trace_channel_new
(
 thread_data_t *thread_data
)
{
  gpu_trace_channel_t *channel
    = hpcrun_malloc_safe(sizeof(gpu_trace_channel_t));

  mpscq_init(&channel->mpsc_queue);
  channel->thread_data = thread_data;
  channel->on_send_fn = NULL;
  channel->on_send_arg = NULL;
  channel->on_receive_fn = NULL;
  channel->on_receive_arg = NULL;

  return channel;
}


void
gpu_trace_channel_init_on_send_callback
(
 gpu_trace_channel_t *channel,
 void (*on_send_fn)(void *),
 void *arg
)
{
  channel->on_send_fn = on_send_fn;
  channel->on_send_arg = arg;
}


void
gpu_trace_channel_init_on_receive_callback
(
 gpu_trace_channel_t *channel,
 void (*on_receive_fn)(void *),
 void *arg
)
{
  channel->on_receive_fn = on_receive_fn;
  channel->on_receive_arg = arg;
}


void
gpu_trace_channel_send
(
 gpu_trace_channel_t *channel,
 const gpu_trace_item_t *trace_item
)
{
  mpscq_entry_t *entry = mpscq_allocate(get_local_allocator());
  entry->trace_item = *trace_item;

  gpu_trace_item_dump(&entry->trace_item, "PRODUCE");

  mpscq_enqueue(&channel->mpsc_queue, entry);

  if (channel->on_send_fn != NULL) {
    channel->on_send_fn(channel->on_send_arg);
  }
}


void
gpu_trace_channel_receive_all
(
 gpu_trace_channel_t *channel,
 void (*receive_fn)(gpu_trace_item_t *, thread_data_t *, void *),
 void *arg
)
{
  for (;;) {
    mpscq_entry_t *entry = mpscq_dequeue(&channel->mpsc_queue);
    if (entry == NULL) {
      break;
    }

    receive_fn(&entry->trace_item, channel->thread_data, arg);
    gpu_trace_item_dump(&entry->trace_item, "CONSUME");
    mpscq_deallocate(entry);

    if (channel != NULL && channel->on_receive_fn != NULL) {
      channel->on_receive_fn(channel->on_receive_arg);
    }
  }
}
