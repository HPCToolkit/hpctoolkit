// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
