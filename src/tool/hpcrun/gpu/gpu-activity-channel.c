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
// Copyright ((c)) 2002-2022, Rice University
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

#include "gpu-activity.h"
#include "gpu-activity-channel.h"
#include "gpu-channel-item-allocator.h"
#include "gpu-channel-common.h"


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

static __thread gpu_activity_channel_t *gpu_activity_channels[GPU_CHANNEL_TOTAL];



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
  return gpu_activity_channel_get_with_idx(0);
}

gpu_activity_channel_t *
gpu_activity_channel_get_with_idx
(
 int idx
)
{
  if (gpu_activity_channels[idx] == NULL) {
    gpu_activity_channels[idx] = gpu_activity_channel_alloc();
  }

  return gpu_activity_channels[idx];
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
 gpu_activity_attribute_fn_t aa_fn
)
{
  return gpu_activity_channel_consume_with_idx(0, aa_fn);
}

void
gpu_activity_channel_consume_with_idx
(
 int idx,
 gpu_activity_attribute_fn_t aa_fn
)
{
  gpu_activity_channel_t *channel = gpu_activity_channel_get_with_idx(idx);

  // steal elements previously enqueued by the producer
  channel_steal(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_activity_t *a = channel_pop(channel, bichannel_direction_forward);
    if (!a) break;
    gpu_activity_consume(a, aa_fn);
    gpu_activity_free(channel, a);
  }
}
