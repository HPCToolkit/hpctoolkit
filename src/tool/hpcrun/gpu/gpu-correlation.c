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
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-correlation.h"
#include "gpu-correlation-channel.h"
#include "gpu-op-placeholders.h"
#include "gpu-channel-item-allocator.h"

#if UNIT_TEST == 0
#include "gpu-host-correlation-map.h"
#endif



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_t {
  s_element_t next;

  // correlation info
  uint64_t host_correlation_id;
  gpu_op_ccts_t gpu_op_ccts;

  uint64_t cpu_submit_time;

  // where to report the activity
  gpu_activity_channel_t *activity_channel;
} gpu_correlation_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_correlation_produce
(
 gpu_correlation_t *c,
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_op_ccts,
 uint64_t cpu_submit_time,
 gpu_activity_channel_t *activity_channel
)
{
  PRINT("Produce correlation id 0x%lx\n", host_correlation_id);
  c->host_correlation_id = host_correlation_id;
  if (gpu_op_ccts) c->gpu_op_ccts = *gpu_op_ccts;
  c->activity_channel = activity_channel;
  c->cpu_submit_time = cpu_submit_time;
}


void
gpu_correlation_consume
(
 gpu_correlation_t *c
)
{
#if UNIT_TEST
    printf("gpu_correlation_consume(%ld, %ld,%ld)\n", c->host_correlation_id);
#else
    PRINT("Consume correlation id 0x%lx\n", c->host_correlation_id);
    gpu_host_correlation_map_insert(c->host_correlation_id, &(c->gpu_op_ccts),
      c->cpu_submit_time, c->activity_channel);
#endif
}


gpu_correlation_t *
gpu_correlation_alloc
(
 gpu_correlation_channel_t *channel
)
{
  return channel_item_alloc(channel, gpu_correlation_t);
}


void
gpu_correlation_free
(
 gpu_correlation_channel_t *channel,
 gpu_correlation_t *c
)
{
  channel_item_free(channel, c);
}

