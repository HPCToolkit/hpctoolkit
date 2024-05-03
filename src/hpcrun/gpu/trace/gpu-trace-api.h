// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_trace_h
#define gpu_trace_h

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../cct/cct.h"
#include "../../thread_data.h"

#include "gpu-trace-channel.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_trace_init
(
 void
);


void
gpu_trace_fini
(
 void *arg,
 int how
);


void *
gpu_trace_record_thread_fn
(
 void *args
);


void
gpu_trace_process_stream
(
 uint32_t device_id,
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_item_t *trace_item
);


void
gpu_trace_process_context
(
 uint32_t context_id,
 gpu_trace_item_t *trace_item
);


void
gpu_set_cpu_gpu_timestamp
(
 uint64_t t1,
 uint64_t t2
);

#endif
