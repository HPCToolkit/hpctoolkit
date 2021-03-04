
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
// Copyright ((c)) 2002-2021, Rice University
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
#include <memory/hpcrun-malloc.h>
#include <pthread.h>


#define DEBUG 0

#include "gpu-trace-channel-set.h"
#include "gpu-trace.h"
#include "gpu-trace-demultiplexer.h"
#include "gpu-print.h"

#include <monitor.h>

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























