// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// global includes
//******************************************************************************

#define _GNU_SOURCE

#include <pthread.h>
#include <limits.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../control-knob.h"
#include <stdatomic.h>

#include "../../control-knob.h"
#include "../trace/gpu-trace-api.h"
#include "../../libmonitor/monitor.h"

#include "../activity/gpu-activity.h"
#include "../activity/gpu-activity-channel.h"

#include "gpu-operation-item.h"
#include "gpu-operation-item-process.h"
#include "gpu-operation-channel-set.h"
#include "gpu-operation-multiplexer.h"

#define DEBUG 0
#include "../common/gpu-print.h"



//******************************************************************************
// local variables
//******************************************************************************

static _Atomic(bool) stop_operation_flag;
static _Atomic(bool) gpu_trace_finished;

static gpu_operation_channel_set_t *channel_set;

static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;



//******************************************************************************
// private operations
//******************************************************************************

void operation_item_process_helper
(
 gpu_operation_item_t *item,
 void *arg
)
{
  gpu_operation_item_process(item);
}


void operation_channel_consume
(
  gpu_operation_channel_t *channel,
  void *arg
)
{
  gpu_operation_channel_receive_all(channel, operation_item_process_helper, NULL);
}


// OpenCL Monitoring thread
static void *
gpu_operation_record_thread_fn
(
 void *arg
)
{
  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

  while (!atomic_load(&stop_operation_flag)) {
    gpu_operation_channel_set_await(channel_set);
    gpu_operation_channel_set_apply(channel_set, operation_channel_consume, NULL);
  }

  gpu_operation_channel_set_apply(channel_set, operation_channel_consume, NULL);

  // even if this is not normal exit, gpu-trace-fini will behave as if it is a normal exit
  gpu_trace_fini(NULL, MONITOR_EXIT_NORMAL);
  atomic_store(&gpu_trace_finished, true);

  return NULL;
}


static void
gpu_operation_multiplexer_create
(
 void
)
{
  atomic_store(&stop_operation_flag, false);
  atomic_store(&gpu_trace_finished, false);

  int max_completion_cb_threads;
  control_knob_value_get_int("MAX_COMPLETION_CALLBACK_THREADS", &max_completion_cb_threads);
  // TODO(Srdjan): use max_completion_cb_threads
  channel_set = gpu_operation_channel_set_new(SIZE_MAX);

  /* Create a monitoring thread */
  monitor_disable_new_threads();
  pthread_t thread; // TODO: store reference to the thread
  pthread_create(&thread, NULL, gpu_operation_record_thread_fn, NULL);
  monitor_enable_new_threads();
}



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_operation_multiplexer_fini
(
 void
)
{
  PRINT("gpu_operation_multiplexer_fini called\n");

  // It may be that we never initialized. Even though we have no data, initialize now so that
  // we can finalize properly.
  pthread_once(&is_initialized, gpu_operation_multiplexer_create);

  atomic_store(&stop_operation_flag, true);

  gpu_operation_channel_set_notify(channel_set);

  while (!atomic_load(&gpu_trace_finished));
}


void
gpu_operation_multiplexer_push
(
 gpu_activity_channel_t *initiator_channel,
 atomic_int *initiator_pending_operations,
 gpu_activity_t *gpu_activity
)
{
  pthread_once(&is_initialized, gpu_operation_multiplexer_create);

  static __thread gpu_operation_channel_t *operation_channel = NULL;
  if (operation_channel == NULL) {
    operation_channel = gpu_operation_channel_get_local();
    gpu_operation_channel_set_add(channel_set, operation_channel);
  }

  gpu_operation_item_t item = {
    .channel=initiator_channel,
    .pending_operations=initiator_pending_operations,
    .activity=*gpu_activity
  };
  gpu_operation_channel_send(operation_channel, &item);
}
