
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


#include <hpcrun/control-knob.h>
#include <lib/prof-lean/stdatomic.h>
#include <pthread.h>


#define DEBUG 0

#include "gpu-activity.h"
#include "gpu-activity-channel.h"
#include "gpu-activity-multiplexer.h"
#include "gpu-activity-process.h"
#include "gpu-monitoring-thread-api.h"
#include "gpu-operation-channel-set.h"
#include "gpu-trace.h"
#include "gpu-print.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef void *(*pthread_start_routine_t)(void *);

//******************************************************************************
// local variables
//******************************************************************************

static _Atomic(bool) stop_activity_flag;
static _Atomic(bool) gpu_trace_finished;

static atomic_uint operation_channels_count;
static __thread uint32_t my_operation_channel_id = -1;
static __thread gpu_operation_channel_t *gpu_operation_channel = NULL;
static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;



//******************************************************************************
// private operations
//******************************************************************************

static void
gpu_init_operation_channel(){
  // Create operation channel
  my_operation_channel_id = atomic_fetch_add(&operation_channels_count, 1);
  gpu_operation_channel = gpu_operation_channel_get();
  gpu_operation_channel_set_insert(gpu_operation_channel, my_operation_channel_id);
}


static void *
gpu_activity_record
(
 void
)
{

  while (!atomic_load(&stop_activity_flag)){
    int current_operation_channels_count = atomic_load(&operation_channels_count);

    for (int set_index = 0; set_index < current_operation_channels_count ; ++set_index) {
      gpu_operation_channel_set_apply(gpu_operation_channel_consume, set_index);

      // TODO: change waiting policy to getting items when full
      gpu_operation_channel_set_apply(gpu_operation_channel_await, set_index);
    }
  }

  int current_operation_channels_count = atomic_load(&operation_channels_count);
  for (int set_index = 0; set_index < current_operation_channels_count; ++set_index) {
    gpu_operation_channel_set_apply(gpu_operation_channel_consume, set_index);
    gpu_operation_channel_set_apply(gpu_operation_channel_await, set_index);
  }

  gpu_trace_fini(NULL);
  atomic_store(&gpu_trace_finished, true);

  return NULL;
}


static void
gpu_activity_multiplexer_create
(
 void
)
{
  pthread_t thread;
  atomic_store(&stop_activity_flag, false);
  atomic_store(&gpu_trace_finished, false);
  atomic_store(&operation_channels_count, 0);

  int max_completion_cb_threads;
  control_knob_value_get_int("MAX_COMPLETION_CALLBACK_THREADS", &max_completion_cb_threads);

  gpu_operation_channel_stack_alloc(max_completion_cb_threads);
  // You are the first to create monitor thread
  pthread_create(&thread, NULL, (pthread_start_routine_t) gpu_activity_record,
                 NULL);
}



//******************************************************************************
// interface operations
//******************************************************************************

bool
gpu_activity_multiplexer_my_channel_initialized
(
 void
)
{
  return (my_operation_channel_id != -1);
}


void
gpu_activity_multiplexer_my_channel_init
(
 void
)
{
  pthread_once(&is_initialized, gpu_activity_multiplexer_create);
  gpu_init_operation_channel();
}


void
gpu_activity_multiplexer_fini
(
 void
)
{
  PRINT("gpu_activity_multiplexer_fini called\n");

  atomic_store(&stop_activity_flag, true);

  int current_operation_channels_count = atomic_load(&operation_channels_count);
  for (int set_index = 0; set_index < current_operation_channels_count; ++set_index) {
    gpu_operation_channel_set_apply(gpu_operation_channel_signal_consumer, set_index);
  }

  while (!atomic_load(&gpu_trace_finished));
}


void
gpu_activity_multiplexer_push
(
 gpu_activity_channel_t *initiator_channel,
 gpu_activity_t *gpu_activity
)
{
  gpu_operation_item_t item = (gpu_operation_item_t){.channel=initiator_channel, .activity=*gpu_activity};
  gpu_operation_channel_produce(gpu_operation_channel, &item);
}























