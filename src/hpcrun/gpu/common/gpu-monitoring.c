// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// attribute GPU performance metrics
//

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "gpu-monitoring.h"



//******************************************************************************
// local variables
//******************************************************************************

static int gpu_inst_sample_frequency = -1;

static int gpu_trace_sample_frequency = -1;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_monitoring_instruction_sample_frequency_set
(
 uint32_t inst_sample_frequency
)
{
  gpu_inst_sample_frequency = inst_sample_frequency;
}


uint32_t
gpu_monitoring_instruction_sample_frequency_get
(
 void
)
{
  return gpu_inst_sample_frequency;
}


void
gpu_monitoring_trace_sample_frequency_set
(
 uint32_t trace_sample_frequency
)
{
  gpu_trace_sample_frequency = trace_sample_frequency;
}


uint32_t
gpu_monitoring_trace_sample_frequency_get
(
 void
)
{
  return gpu_trace_sample_frequency;
}
