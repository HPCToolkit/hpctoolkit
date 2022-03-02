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

//******************************************************************************
// local includes
//******************************************************************************


#include <lib/prof-lean/bichannel.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-activity-channel.h"
#include "gpu-correlation.h"
#include "gpu-correlation-channel.h"
#include "gpu-correlation-channel-set.h"



//******************************************************************************
// macros
//******************************************************************************

#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) gpu_correlation_channel_t
#define typed_stack_elem(x) gpu_correlation_t

// define macros that simplify use of correlation channel API
#define channel_init  \
  typed_bichannel_init(gpu_correlation_t)

#define channel_pop   \
  typed_bichannel_pop(gpu_correlation_t)

#define channel_push  \
  typed_bichannel_push(gpu_correlation_t)

#define channel_steal \
  typed_bichannel_steal(gpu_correlation_t)



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t {
  bistack_t bistacks[2];
} gpu_correlation_channel_t;



//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_correlation_channel_t *gpu_correlation_channels[GPU_CHANNEL_TOTAL];



//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for correlations
typed_bichannel_impl(gpu_correlation_t)


static gpu_correlation_channel_t *
gpu_correlation_channel_alloc_with_idx
(
 int idx
)
{
  gpu_correlation_channel_t *c =
    hpcrun_malloc_safe(sizeof(gpu_correlation_channel_t));

  channel_init(c);

  gpu_correlation_channel_set_insert_with_idx(idx, c);

  return c;
}


static gpu_correlation_channel_t *
gpu_correlation_channel_get_with_idx
(
 int idx
)
{
  if (gpu_correlation_channels[idx] == NULL) {
    gpu_correlation_channels[idx] = gpu_correlation_channel_alloc_with_idx(idx);
  }

  return gpu_correlation_channels[idx];
}

//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_correlation_channel_produce
(
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_op_ccts,
 uint64_t cpu_submit_time
)
{
  // Relaying parameters with index 0
  gpu_correlation_channel_produce_with_idx(0, host_correlation_id, gpu_op_ccts, cpu_submit_time);
}

void
gpu_correlation_channel_produce_with_idx
(
 int idx,
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_op_ccts,
 uint64_t cpu_submit_time
)
{
  gpu_correlation_channel_t *corr_channel = gpu_correlation_channel_get_with_idx(idx);
  gpu_activity_channel_t *activity_channel = gpu_activity_channel_get_with_idx(idx);

  gpu_correlation_t *c = gpu_correlation_alloc(corr_channel);

  gpu_correlation_produce(c, host_correlation_id, gpu_op_ccts, cpu_submit_time,
			  activity_channel);

  channel_push(corr_channel, bichannel_direction_forward, c);
}

void
gpu_correlation_channel_consume
(
 gpu_correlation_channel_t *channel
)
{
  // steal elements previously enqueued by the producer
  channel_steal(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    gpu_correlation_t *c = channel_pop(channel, bichannel_direction_forward);
    if (!c) break;
    gpu_correlation_consume(c);
    gpu_correlation_free(channel, c);
  }
}


//******************************************************************************
// unit testing
//******************************************************************************


#define UNIT_TEST 0

#if UNIT_TEST

#include <stdlib.h>
#include "gpu-correlation-channel-set.h"
#include "gpu-op-placeholders.h"


void *hpcrun_malloc_safe
(
 size_t s
)
{
  return malloc(s);
}


gpu_activity_channel_t *
gpu_activity_channel_get
(
 void
)
{
  return (gpu_activity_channel_t *) 0x5000;
}


int
main
(
 int argc,
 char **argv
)
{
  gpu_op_ccts_t gpu_op_ccts;

  int i;
  for(i = 0; i < 10; i++) {
    memset(&gpu_op_ccts, i, sizeof(gpu_op_ccts_t));
    gpu_correlation_channel_produce(i, &gpu_op_ccts);
  }
  gpu_correlation_channel_set_consume();
  for(i = 20; i < 30; i++) {
    memset(&gpu_op_ccts, i, sizeof(gpu_op_ccts_t));
    gpu_correlation_channel_produce(i, &gpu_op_ccts);
  }
  gpu_correlation_channel_set_consume();
}

#endif
