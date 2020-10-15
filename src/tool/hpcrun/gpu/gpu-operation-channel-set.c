
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


//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stacks.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/thread_data.h>


#include "gpu-activity-process.h"
#include "gpu-operation-multiplexer.h"
#include "gpu-operation-item.h"
#include "gpu-operation-channel.h"
#include "gpu-operation-channel-set.h"
#include "gpu-print.h"




//******************************************************************************
// macros
//******************************************************************************

#define channel_stack_push  \
  typed_stack_push(gpu_operation_channel_ptr_t, cstack)

#define channel_stack_forall \
  typed_stack_forall(gpu_operation_channel_ptr_t, cstack)

#define channel_stack_elem_t \
  typed_stack_elem(gpu_operation_channel_ptr_t)

#define channel_stack_elem_ptr_set \
  typed_stack_elem_ptr_set(gpu_operation_channel_ptr_t, cstack)



//******************************************************************************
// type declarations
//******************************************************************************

//----------------------------------------------------------
// support for a stack of operation channels
//----------------------------------------------------------

typedef gpu_operation_channel_t* gpu_operation_channel_ptr_t;


typedef struct {
  s_element_ptr_t next;
  gpu_operation_channel_ptr_t channel;
} typed_stack_elem(gpu_operation_channel_ptr_t);


typed_stack_declare_type(gpu_operation_channel_ptr_t);



//******************************************************************************
// local data
//******************************************************************************

static
typed_stack_elem_ptr(gpu_operation_channel_ptr_t) *gpu_operation_channel_stack;



//******************************************************************************
// private operations
//******************************************************************************

// implement stack of operation channels
typed_stack_impl(gpu_operation_channel_ptr_t, cstack);


static void
channel_forone
(
 channel_stack_elem_t *se,
 void *arg
)
{
  gpu_operation_channel_t *channel = se->channel;

  gpu_operation_channel_fn_t channel_fn = (gpu_operation_channel_fn_t) arg;

  channel_fn(channel);
}


static void
gpu_operation_channel_set_forall
(
 gpu_operation_channel_fn_t channel_fn,
 int set_index
)
{
  channel_stack_forall(&gpu_operation_channel_stack[set_index], channel_forone,
                       channel_fn);
}


static void
gpu_operation_channel_set_apply
(
gpu_operation_channel_fn_t channel_fn,
int channel_num
)
{
  for (int channel_idx = 0; channel_idx < channel_num; ++channel_idx) {
    gpu_operation_channel_set_forall(channel_fn, channel_idx);
  }
}


//******************************************************************************
// interface operations
//******************************************************************************

void gpu_operation_channel_set_alloc(int size){
  gpu_operation_channel_stack = hpcrun_malloc_safe( size * sizeof(typed_stack_elem_ptr(gpu_operation_channel_ptr_t)));
}

void
gpu_operation_channel_set_insert
(
 gpu_operation_channel_t *channel,
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
  channel_stack_push(&gpu_operation_channel_stack[set_index], e);
}


void
gpu_operation_channel_set_process
(
 int channel_num
)
{
  gpu_operation_channel_set_apply(gpu_operation_channel_consume, channel_num);
}


void
gpu_operation_channel_set_await
(
 int channel_num
)
{
  gpu_operation_channel_set_apply(gpu_operation_channel_await, channel_num);
}


void
gpu_operation_channel_set_notify
(
 int channel_num
)
{
  gpu_operation_channel_set_apply(gpu_operation_channel_signal_consumer, channel_num);
}
