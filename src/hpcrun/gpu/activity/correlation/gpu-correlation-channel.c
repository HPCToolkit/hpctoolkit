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
// Copyright ((c)) 2002-2024, Rice University
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
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../../common/prof-lean/collections/mpsc-queue-entry-data.h"

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
#include "../../../../common/prof-lean/collections/mpsc-queue.h"



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
