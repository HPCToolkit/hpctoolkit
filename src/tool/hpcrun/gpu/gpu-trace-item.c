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
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-channel-item-allocator.h"
#include "gpu-trace-item.h"



//******************************************************************************
// interface operations 
//******************************************************************************

void
gpu_trace_item_produce
(
 gpu_trace_item_t *ti,
 uint64_t cpu_submit_time,
 uint64_t start,
 uint64_t end,
 cct_node_t *call_path_leaf
)
{
  ti->cpu_submit_time = cpu_submit_time;
  ti->start = start;
  ti->end = end;
  ti->call_path_leaf = call_path_leaf;
  cstack_ptr_set(&(ti->next), 0);
}


void
gpu_trace_item_consume
(
 gpu_trace_item_consume_fn_t trace_item_consume,
 thread_data_t *td,
 gpu_trace_item_t *ti
)
{
  trace_item_consume(td, ti->call_path_leaf, ti->start, ti->end);
}


gpu_trace_item_t *
gpu_trace_item_alloc
(
 gpu_trace_channel_t *channel
)
{
  return channel_item_alloc(channel, gpu_trace_item_t);
}


void
gpu_trace_item_free
(
 gpu_trace_channel_t *channel, 
 gpu_trace_item_t *ti
)
{
  channel_item_free(channel, ti);
}

