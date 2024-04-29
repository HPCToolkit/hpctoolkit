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

#include "../../../common/lean/collections/mpsc-queue-entry-data.h"

#include "../../memory/hpcrun-malloc.h"

#include "gpu-operation-channel.h"
#include "gpu-operation-item.h"

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// generic code - MPSC queue
//******************************************************************************

typedef struct mpscq_entry_t {
  MPSC_QUEUE_ENTRY_DATA(struct mpscq_entry_t);
  gpu_operation_item_t operation_item;
} mpscq_entry_t;


#define MPSC_QUEUE_PREFIX         mpscq
#define MPSC_QUEUE_ENTRY_TYPE     mpscq_entry_t
#define MPSC_QUEUE_ALLOCATE       hpcrun_malloc_safe
#include "../../../common/lean/collections/mpsc-queue.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_operation_channel_t {
  mpscq_t mpsc_queue;

  void (*on_send_fn)(void *);
  void *on_send_arg;

  void (*on_receive_fn)(void *);
  void *on_receive_arg;
} gpu_operation_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_operation_channel_t *channel;



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

gpu_operation_channel_t *
gpu_operation_channel_get_local
(
 void
)
{
  if (channel == NULL) {
    channel = hpcrun_malloc_safe(sizeof(gpu_operation_channel_t));
    *channel = (gpu_operation_channel_t) {
      .mpsc_queue = MPSC_QUEUE_INITIALIZER,
      .on_send_arg = NULL,
      .on_send_arg = NULL,
      .on_receive_fn = NULL,
      .on_receive_arg = NULL
    };
  }

  return channel;
}


void
gpu_operation_channel_init_on_send_callback
(
 gpu_operation_channel_t *channel,
 void (*on_send_fn)(void *),
 void *arg
)
{
  channel->on_send_fn = on_send_fn;
  channel->on_send_arg = arg;
}


void
gpu_operation_channel_init_on_receive_callback
(
 gpu_operation_channel_t *channel,
 void (*on_receive_fn)(void *),
 void *arg
)
{
  channel->on_receive_fn = on_receive_fn;
  channel->on_receive_arg = arg;
}


void
gpu_operation_channel_send
(
 gpu_operation_channel_t *channel,
 const gpu_operation_item_t *item
)
{
  mpscq_entry_t *entry = mpscq_allocate(get_local_allocator());
  entry->operation_item = *item;

  gpu_operation_item_dump(&entry->operation_item, "PRODUCE");

  mpscq_enqueue(&channel->mpsc_queue, entry);

  if (channel->on_send_fn != NULL) {
    channel->on_send_fn(channel->on_send_arg);
  }
}


void
gpu_operation_channel_receive_all
(
 gpu_operation_channel_t *channel,
 void (*receive_fn)(gpu_operation_item_t *, void *),
 void *arg
)
{
  for (;;) {
    mpscq_entry_t *entry = mpscq_dequeue(&channel->mpsc_queue);
    if (entry == NULL) {
      break;
    }

    receive_fn(&entry->operation_item, arg);
    gpu_operation_item_dump(&entry->operation_item, "CONSUME");
    mpscq_deallocate(entry);

    if (channel != NULL && channel->on_receive_fn != NULL) {
      channel->on_receive_fn(channel->on_receive_arg);
    }
  }
}
