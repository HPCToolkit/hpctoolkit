// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../../common/lean/collections/mpsc-queue-entry-data.h"

#include "../../../memory/hpcrun-malloc.h"

#include "gpu-channel-common.h"
#include "gpu-correlation-channel.h"

#define DEBUG 0
#include "../../common/gpu-print.h"



//******************************************************************************
// generic code - MPSC queue
//******************************************************************************

typedef struct mpscq_entry_t {
  MPSC_QUEUE_ENTRY_DATA(struct mpscq_entry_t);

  uint64_t host_correlation_id;
  gpu_activity_channel_t *activity_channel;
} mpscq_entry_t;


#define MPSC_QUEUE_PREFIX         mpscq
#define MPSC_QUEUE_ENTRY_TYPE     mpscq_entry_t
#define MPSC_QUEUE_ALLOCATE       hpcrun_malloc_safe
#include "../../../../common/lean/collections/mpsc-queue.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t {
  mpscq_t mpsc_queue;
} gpu_correlation_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static gpu_correlation_channel_t correlation_channels[] = {
  [0 ... (GPU_CHANNEL_TOTAL - 1)] = { .mpsc_queue = MPSC_QUEUE_INITIALIZER }
};



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

void
gpu_correlation_channel_send
(
 uint64_t idx,
 uint64_t host_correlation_id,
 gpu_activity_channel_t *activity_channel
)
{
  gpu_correlation_channel_t *correlation_channel = &correlation_channels[idx];

  mpscq_entry_t *entry = mpscq_allocate(get_local_allocator());
  entry->host_correlation_id = host_correlation_id;
  entry->activity_channel = activity_channel;

  PRINT("CORRELATION_CHANNEL_PRODUCE host_correlation_id 0x%lx, activity_channel = %p\n",
    host_correlation_id,
    activity_channel);

  mpscq_enqueue(&correlation_channel->mpsc_queue, entry);
}


void
gpu_correlation_channel_receive
(
 uint64_t idx,
 void (*receive_fn)(uint64_t, gpu_activity_channel_t *, void *),
 void *arg
)
{
  gpu_correlation_channel_t *correlation_channel = &correlation_channels[idx];

  for (;;) {
    mpscq_entry_t *entry = mpscq_dequeue(&correlation_channel->mpsc_queue);
    if (entry == NULL) {
      break;
    }

    receive_fn(entry->host_correlation_id, entry->activity_channel, arg);
    PRINT("CORRELATION_CHANNEL_PRODUCE host_correlation_id 0x%lx, activity_channel = %p\n",
      entry->host_correlation_id,
      entry->activity_channel);
    mpscq_deallocate(entry);
  }
}
