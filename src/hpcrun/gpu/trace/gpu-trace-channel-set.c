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
// macros
//******************************************************************************

#define BASE(DERIVED) (&DERIVED->base)

#define SECONDS_UNTIL_WAKEUP 1
#define UNPROCESSED_TARGET_COUNT 100



//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../common/lean/collections/concurrent-stack-entry-data.h"

#include "../../thread_data.h"
#include "../../memory/hpcrun-malloc.h"

#include "../common/base-channel-set.h"

#include "gpu-trace-channel.h"
#include "gpu-trace-channel-set.h"

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// generic code - macro expansions
//******************************************************************************

typedef struct channel_stack_entry_t {
  CONCURRENT_STACK_ENTRY_DATA(struct channel_stack_entry_t);
  gpu_trace_channel_t *channel;
} channel_stack_entry_t;


#define CONCURRENT_STACK_PREFIX         channel_stack
#define CONCURRENT_STACK_ENTRY_TYPE     channel_stack_entry_t
#include "../../../common/lean/collections/concurrent-stack.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_channel_set_t {
  base_channel_set_t base;
  channel_stack_t channels;
} gpu_trace_channel_set_t;


typedef struct apply_callback_helper_data_t {
  void (*apply_fn)(gpu_trace_channel_t *, void *);
  void *arg;
} apply_callback_helper_data_t;



//******************************************************************************
// private operations
//******************************************************************************

static void
apply_callback_helper
(
 channel_stack_entry_t *entry,
 void *arg
)
{
  apply_callback_helper_data_t *data = arg;
  data->apply_fn(entry->channel, data->arg);
}



//******************************************************************************
// interface operations
//******************************************************************************

gpu_trace_channel_set_t *
gpu_trace_channel_set_new
(
 size_t max_channels
)
{
  gpu_trace_channel_set_t *channel_set
    = hpcrun_malloc_safe(sizeof(gpu_trace_channel_set_t));

  base_channel_set_init(BASE(channel_set), max_channels, UNPROCESSED_TARGET_COUNT);
  channel_stack_init(&channel_set->channels);

  return channel_set;
}


_Bool
gpu_trace_channel_set_add
(
 gpu_trace_channel_set_t *channel_set,
 gpu_trace_channel_t *channel
)
{
  if (!base_channel_set_add(BASE(channel_set))) {
    return 0;
  }

  /* Register callbacks*/
  gpu_trace_channel_init_on_send_callback(channel,
    base_channel_set_on_send_callback, BASE(channel_set));
  gpu_trace_channel_init_on_receive_callback(channel,
    base_channel_set_on_receive_callback, BASE(channel_set));

  /* Allocate a new channel entry and add it to the stack*/
  /* TODO(Srdjan): Who should allocate memory? (this can be invoked from multiple threads)*/
  channel_stack_entry_t *channel_entry
    = hpcrun_malloc_safe(sizeof(channel_stack_entry_t));

  channel_entry->channel = channel;
  channel_stack_push(&channel_set->channels, channel_entry);

  return 1;
}


void
gpu_trace_channel_set_await
(
 gpu_trace_channel_set_t *channel_set
)
{
  base_channel_set_await(BASE(channel_set), SECONDS_UNTIL_WAKEUP * 1000000);
}


void
gpu_trace_channel_set_notify
(
 gpu_trace_channel_set_t *channel_set
)
{
  base_channel_set_notify(BASE(channel_set));
}


void
gpu_trace_channel_set_apply
(
 gpu_trace_channel_set_t *channel_set,
 void (*apply_fn)(gpu_trace_channel_t *, void *),
 void *arg
)
{
  apply_callback_helper_data_t data = {
    .apply_fn = apply_fn,
    .arg = arg
  };
  channel_stack_for_each(&channel_set->channels, apply_callback_helper, &data);
}
