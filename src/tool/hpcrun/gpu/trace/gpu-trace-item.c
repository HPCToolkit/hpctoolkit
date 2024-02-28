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

#define _GNU_SOURCE

//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0
#define DEBUG 0



//******************************************************************************
// system includes
//******************************************************************************

#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-trace-item.h"
#include "../common/gpu-print.h"
#include "../../messages/messages.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_trace_item_init
(
 gpu_trace_item_t *trace_item,
 uint64_t cpu_submit_time,
 uint64_t start,
 uint64_t end,
 cct_node_t *call_path_leaf
)
{
  trace_item->cpu_submit_time = cpu_submit_time;
  trace_item->start = start;
  trace_item->end = end;
  trace_item->call_path_leaf = call_path_leaf;
    if (call_path_leaf == NULL)
    hpcrun_terminate();
}


void
gpu_trace_item_dump
(
  const gpu_trace_item_t *trace_item,
  const char *context
)
{
  PRINT("===========TRACE_%s: trace_item = %p || submit = %lu, start = %lu, end = %lu, cct_node = %p\n",
        context,
        trace_item,
        trace_item->cpu_submit_time,
        trace_item->start,
        trace_item->end,
        trace_item->call_path_leaf);
}
