
// * BeginRiceCopyright *****************************************************
// -*-Mode: C++;-*- // technically C99
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
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../common/lean/collections/concurrent-stack-entry-data.h"

#include "../../control-knob.h"
#include "../../memory/hpcrun-malloc.h"
#include "../../messages/messages.h"
#include "../../libmonitor/monitor.h"

#include "gpu-trace-channel.h"
#include "gpu-trace-channel-set.h"
#include "gpu-trace-api.h"
#include "gpu-trace-demultiplexer.h"

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// generic code - macro expansions
//******************************************************************************

typedef struct channel_set_entry_t {
  CONCURRENT_STACK_ENTRY_DATA(struct channel_set_entry_t);
  gpu_trace_channel_set_t *channel_set;
  pthread_t thread;
} channel_set_entry_t;


#define CONCURRENT_STACK_PREFIX         channel_set_stack
#define CONCURRENT_STACK_ENTRY_TYPE     channel_set_entry_t
#include "../../../common/lean/collections/concurrent-stack.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct trace_demux_t {
  channel_set_stack_t channel_sets;
  int streams_per_thread;
} trace_demux_t;



//***************************************************************************
// local variables
//***************************************************************************

pthread_once_t demux_is_initialized = PTHREAD_ONCE_INIT;
trace_demux_t demux;



//******************************************************************************
// private operations
//******************************************************************************

static channel_set_entry_t *
gpu_trace_channel_set_create
(
 int streams_per_thread
)
{
  channel_set_entry_t *entry =
    hpcrun_malloc_safe(sizeof(channel_set_entry_t));

  entry->channel_set = gpu_trace_channel_set_new(streams_per_thread);

  /* Create a tracing thread */
  monitor_disable_new_threads();
  pthread_create(&entry->thread, NULL, gpu_trace_record_thread_fn, entry->channel_set);
  monitor_enable_new_threads();

  return entry;
}


static void
demux_init
(
 void
)
{
  int streams_per_thread;
  control_knob_value_get_int("STREAMS_PER_TRACING_THREAD", &streams_per_thread);
  if (streams_per_thread < 1) {
    TMSG(TRACE, "ERROR: STREAMS_PER_TRACING_THREAD is less than 1 (%d)\n", streams_per_thread);
    streams_per_thread = 16;
  }
  PRINT("streams_per_thread = %d \n", streams_per_thread);

  channel_set_stack_init(&demux.channel_sets);
  channel_set_stack_push(&demux.channel_sets, gpu_trace_channel_set_create(streams_per_thread));
  demux.streams_per_thread = streams_per_thread;
}


static trace_demux_t*
gpu_trace_demultiplexer_get
(
 void
)
{
  pthread_once(&demux_is_initialized, demux_init);
  return &demux;
}


static void
notify_iter_callback
(
 channel_set_entry_t *channel_set,
 void *arg
)
{
  gpu_trace_channel_set_notify(channel_set->channel_set);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_trace_demultiplexer_push
(
 gpu_trace_channel_t *trace_channel
)
{
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  trace_demux_t *demux = gpu_trace_demultiplexer_get();

  gpu_trace_channel_set_t *channel_set =
    channel_set_stack_top(&demux->channel_sets)->channel_set;

  /* If all channel sets are full, create a new channel set.
     Use mutex to ensure that exactly one new channel set is created. */
  if (!gpu_trace_channel_set_add(channel_set, trace_channel)) {
    pthread_mutex_lock(&mtx);

    channel_set = channel_set_stack_top(&demux->channel_sets)->channel_set;
    if (!gpu_trace_channel_set_add(channel_set, trace_channel)) {
      /* Create a new channel set and push the current channel to it. Since no
         other thread can access the new channel set, the push cannot fail. */
      channel_set_entry_t *channel_set_entry =
              gpu_trace_channel_set_create(demux->streams_per_thread);
      channel_set = channel_set_entry->channel_set;
      gpu_trace_channel_set_add(channel_set, trace_channel);

      /* Make the newly created channel set visible for other threads. */
      channel_set_stack_push(&demux->channel_sets, channel_set_entry);
    }

    pthread_mutex_unlock(&mtx);
  }

  PRINT("gpu_trace_demultiplexer_push: channel_set = %p | channel = %p\n",
        channel_set, trace_channel);
}


void
gpu_trace_demultiplexer_notify
(
 void
)
{
  trace_demux_t *demux = gpu_trace_demultiplexer_get();
  channel_set_stack_for_each(&demux->channel_sets, notify_iter_callback, NULL);
}
