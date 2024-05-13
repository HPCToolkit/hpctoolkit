// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "../control-knob.h"
#include <stdatomic.h>
#include "../memory/hpcrun-malloc.h"
#include <pthread.h>


#define DEBUG 0

#include "trace/gpu-trace-channel-set.h"
#include <gpu-trace.h>
#include "trace/gpu-trace-demultiplexer.h"
#include "common/gpu-print.h"

#include "../libmonitor/monitor.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef void *(*pthread_start_routine_t)(void *);

typedef struct gpu_trace_channel_set_t{
  void *channel_set_ptr;
  gpu_trace_channel_set_t *next;
  pthread_t thread;
  atomic_uint channel_index;
}gpu_trace_channel_set_t;



//******************************************************************************
// local variables
//******************************************************************************

static gpu_trace_channel_set_t *trace_channel_set_list_head = NULL;
static gpu_trace_channel_set_t *trace_channel_set_list_tail = NULL;

static int streams_per_thread;



//******************************************************************************
// private operations
//******************************************************************************

static gpu_trace_channel_set_t *
gpu_trace_channel_set_create
(
 void
)
{
  gpu_trace_channel_set_t *new_channel_set= hpcrun_malloc(sizeof(gpu_trace_channel_set_t));
  new_channel_set->next = NULL;
  new_channel_set->channel_set_ptr = gpu_trace_channel_set_alloc(streams_per_thread);
  atomic_store(&new_channel_set->channel_index, 0);
  monitor_disable_new_threads();
  // Create tracing thread
  pthread_create(&new_channel_set->thread, NULL, (pthread_start_routine_t) gpu_trace_record,
                 new_channel_set);
  monitor_enable_new_threads();
  return new_channel_set;
}


static void
gpu_trace_demultiplexer_init
(
 void
)
{
  control_knob_value_get_int("STREAMS_PER_TRACING_THREAD", &streams_per_thread);
  trace_channel_set_list_head = gpu_trace_channel_set_create();
  trace_channel_set_list_tail = trace_channel_set_list_head;

  PRINT("streams_per_thread = %d \n", streams_per_thread);
}



//******************************************************************************
// interface operations
//******************************************************************************

void *
gpu_trace_channel_set_get_ptr
(
 gpu_trace_channel_set_t *channel_set
)
{
  return channel_set->channel_set_ptr;
}


uint32_t
gpu_trace_channel_set_get_channel_num
(
 gpu_trace_channel_set_t *channel_set
)
{
  return atomic_load(&channel_set->channel_index);
}


pthread_t
gpu_trace_channel_set_get_thread
(
gpu_trace_channel_set_t *channel_set
)
{
  return channel_set->thread;
}


pthread_t
gpu_trace_demultiplexer_push
(
 gpu_trace_channel_t *trace_channel
)
{

  if (trace_channel_set_list_head == NULL){
    gpu_trace_demultiplexer_init();
  }

  if (atomic_load(&trace_channel_set_list_tail->channel_index) == streams_per_thread){
    // Create new channel_set
    trace_channel_set_list_tail->next = gpu_trace_channel_set_create();
    trace_channel_set_list_tail = trace_channel_set_list_tail->next;
  }

  gpu_trace_channel_set_insert(trace_channel,
                               trace_channel_set_list_tail->channel_set_ptr,
                               atomic_fetch_add(&trace_channel_set_list_tail->channel_index,1));

  PRINT("gpu_trace_demultiplexer_push: channel_set_ptr = %p | channel = %p\n",
        trace_channel_set_list_tail->channel_set_ptr,
        trace_channel);

  return trace_channel_set_list_tail->thread;
}


void
gpu_trace_demultiplexer_notify
(
 void
)
{
  gpu_trace_channel_set_t *iter;

  for (iter = trace_channel_set_list_head; iter != trace_channel_set_list_tail; iter = iter->next){
    gpu_trace_channel_set_notify(iter);
  }
}


void
gpu_trace_demultiplexer_release
(
 void
)
{
  PRINT("gpu_trace_demultiplexer_release: NOT IMPLEMENTED\n");
}
