// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../common/lean/mcs-lock.h"
#include "../../../common/lean/collections/mpsc-queue-entry-data.h"

#include "../../memory/hpcrun-malloc.h"

#include "gpu-activity.h"
#include "gpu-activity-channel.h"



//******************************************************************************
// generic code - MPSC queue
//******************************************************************************

typedef struct mpscq_entry_t {
  MPSC_QUEUE_ENTRY_DATA(struct mpscq_entry_t);
  gpu_activity_t activity;
} mpscq_entry_t;


#define MPSC_QUEUE_PREFIX         mpscq
#define MPSC_QUEUE_ENTRY_TYPE     mpscq_entry_t
#define MPSC_QUEUE_ALLOCATE       hpcrun_malloc_safe
#include "../../../common/lean/collections/mpsc-queue.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_channel_t {
  mpscq_t base;
} gpu_activity_channel_t;



//******************************************************************************
// generic code - concurrent ID map
//******************************************************************************

/* Instantiate concurrent ID map */
#define CONCURRENT_ID_MAP_PREFIX                ch_map
#define CONCURRENT_ID_MAP_ENTRY_INITIALIZER     { .base = MPSC_QUEUE_INITIALIZER }
#define CONCURRENT_ID_MAP_ENTRY_TYPE            gpu_activity_channel_t
#include "../../../common/lean/collections/concurrent-id-map.h"



//******************************************************************************
// local data
//******************************************************************************

static atomic_uint_least32_t next_channel_id = ATOMIC_VAR_INIT(1u);
static __thread uint32_t local_channel_id = 0;

static ch_map_t channel_map = CONCURRENT_ID_MAP_INITIALIZER(hpcrun_malloc_safe);



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
// interface operations
//******************************************************************************

uint64_t
gpu_activity_channel_generate_correlation_id
(
 void
)
{
  // Local ID
  static __thread uint32_t local_thread_next_id = 0;
  if (++local_thread_next_id == 0) {
    local_thread_next_id = 1;
  }

  // Channel ID (Thread ID)
  if (local_channel_id == 0) {
    gpu_activity_channel_get_local();
  }

  uint64_t correlation_id = ((uint64_t) local_channel_id << 32) | local_thread_next_id;
  return correlation_id;
}


uint32_t
gpu_activity_channel_correlation_id_get_thread_id
(
 uint64_t correlation_id
)
{
  return correlation_id >> 32;
}


gpu_activity_channel_t *
gpu_activity_channel_get_local
(
 void
)
{
  if (local_channel_id == 0) {
    local_channel_id = atomic_fetch_add(&next_channel_id, 1);
  }

  return ch_map_get(&channel_map, local_channel_id);
}


gpu_activity_channel_t *
gpu_activity_channel_lookup
(
 uint32_t thread_id
)
{
  return ch_map_get(&channel_map, thread_id);
}


void
gpu_activity_channel_send
(
 gpu_activity_channel_t *channel,
 const gpu_activity_t *activity
)
{
  mpscq_entry_t *entry = mpscq_allocate(get_local_allocator());
  entry->activity = *activity;

  gpu_context_activity_dump(&entry->activity, "PRODUCE");

  mpscq_enqueue(&channel->base, entry);
}


void
gpu_activity_channel_receive_all
(
 void (*receive_fn)(gpu_activity_t *)
)
{
  gpu_activity_channel_t *channel = gpu_activity_channel_get_local();

  for (;;) {
    mpscq_entry_t *entry = mpscq_dequeue(&channel->base);
    if (entry == NULL) {
      break;
    }
    receive_fn(&entry->activity);
    gpu_context_activity_dump(&entry->activity, "CONSUME");
    mpscq_deallocate(entry);
  }
}
