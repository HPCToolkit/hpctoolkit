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
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-channel-item-allocator.h"
#include "gpu-operation-channel.h"
#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"


//******************************************************************************
// macros
//******************************************************************************

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

#define gpu_operation_item_alloc(channel)		\
  channel_item_alloc(channel, gpu_operation_item_t)

#define gpu_operation_item_free(channel, item)	\
  channel_item_free(channel, item)


//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_operation_channel_t {
  bistack_t bistacks[2];
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


static gpu_operation_channel_t *
gpu_operation_channel_alloc
(
void
)
{
  gpu_operation_channel_t *c = hpcrun_malloc_safe(sizeof(gpu_operation_channel_t));

  channel_init(c);

  return c;
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
  gpu_operation_item_t *channel_op = gpu_operation_item_alloc(channel);
  *channel_op = *it;

  channel_push(channel, bichannel_direction_forward, channel_op);
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
    gpu_operation_item_consume(gpu_operation_item_process, it);
    gpu_operation_item_free(channel, it);
  }
}
