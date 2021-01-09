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

#include <lib/prof-lean/stacks.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/control-knob.h>


#define DEBUG 0

#include "gpu-print.h"
#include "gpu-trace.h"
#include "gpu-trace-channel.h"
#include "gpu-trace-channel-set.h"
#include "gpu-trace-demultiplexer.h"



//******************************************************************************
// macros
//******************************************************************************

#define channel_stack_push  \
  typed_stack_push(gpu_trace_channel_ptr_t, cstack)

#define channel_stack_forall \
  typed_stack_forall(gpu_trace_channel_ptr_t, cstack)

#define channel_stack_elem_t \
  typed_stack_elem(gpu_trace_channel_ptr_t)

#define channel_stack_elem_ptr_set \
  typed_stack_elem_ptr_set(gpu_trace_channel_ptr_t, cstack)



//******************************************************************************
// type declarations
//******************************************************************************

//----------------------------------------------------------
// support for a stack of trace channels
//----------------------------------------------------------

typedef gpu_trace_channel_t* gpu_trace_channel_ptr_t;

typedef struct {
  s_element_ptr_t next;
  gpu_trace_channel_ptr_t channel;
} typed_stack_elem(gpu_trace_channel_ptr_t);


typed_stack_declare_type(gpu_trace_channel_ptr_t);



//******************************************************************************
// local data
//******************************************************************************

//******************************************************************************
// private operations
//******************************************************************************

// implement stack of trace channels
typed_stack_impl(gpu_trace_channel_ptr_t, cstack);


static void
channel_forone
(
 channel_stack_elem_t *se,
 void *arg
)
{
  gpu_trace_channel_t *channel = se->channel;

  gpu_trace_channel_fn_t channel_fn =
    (gpu_trace_channel_fn_t) arg;

  channel_fn(channel);
}


static void
gpu_trace_channel_set_forall
(
 gpu_trace_channel_fn_t channel_fn,
 typed_stack_elem_ptr(gpu_trace_channel_ptr_t) *gpu_trace_channel_stack,
 int set_index
)
{
  channel_stack_forall(&gpu_trace_channel_stack[set_index], channel_forone,
    channel_fn);
}


static void
gpu_trace_channel_set_apply
(
 gpu_trace_channel_fn_t channel_fn,
 gpu_trace_channel_set_t *channel_set
)
{
  int channel_count = gpu_trace_channel_set_get_channel_num(channel_set);
  typed_stack_elem_ptr(gpu_trace_channel_ptr_t) * gpu_trace_channel_stack = gpu_trace_channel_set_get_ptr(channel_set);

  for (int channel_idx = 0; channel_idx < channel_count; ++channel_idx) {
    gpu_trace_channel_set_forall(channel_fn,
                                 gpu_trace_channel_stack,
                                 channel_idx);
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

void *
gpu_trace_channel_set_alloc(int size){
  return hpcrun_malloc_safe( size * sizeof(typed_stack_elem_ptr(gpu_trace_channel_ptr_t)));
}


void
gpu_trace_channel_set_insert
(
 gpu_trace_channel_t *channel,
 void *gpu_trace_channel_stack_ptr,
 int set_index
)
{
  // allocate and initialize new entry for channel stack
  channel_stack_elem_t *e = 
    (channel_stack_elem_t *) hpcrun_malloc_safe(sizeof(channel_stack_elem_t));

  // initialize the new entry
  e->channel = channel;

  // clear the entry's next ptr
  channel_stack_elem_ptr_set(e, 0);

	  // add the entry to the channel stack
  typed_stack_elem_ptr(gpu_trace_channel_ptr_t) * gpu_trace_channel_stack = gpu_trace_channel_stack_ptr;
  channel_stack_push(&gpu_trace_channel_stack[set_index], e);
}


void
gpu_trace_channel_set_process
(
 gpu_trace_channel_set_t *channel_set
)
{
  gpu_trace_channel_set_apply(gpu_trace_channel_consume, channel_set);
}


void
gpu_trace_channel_set_await
(
 gpu_trace_channel_set_t *channel_set
)
{
  gpu_trace_channel_set_apply(gpu_trace_channel_await, channel_set);
}


void
gpu_trace_channel_set_release
(
 gpu_trace_channel_set_t *channel_set
)
{
  gpu_trace_channel_set_apply(gpu_trace_stream_release, channel_set);
}


void
gpu_trace_channel_set_notify
(
gpu_trace_channel_set_t *channel_set
)
{
  gpu_trace_channel_set_apply(gpu_trace_channel_signal_consumer, channel_set);
}