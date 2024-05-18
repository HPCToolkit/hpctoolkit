// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_monitoring_h
#define gpu_monitoring_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_monitoring_instruction_sample_frequency_set
(
 uint32_t inst_sample_frequency
);


uint32_t
gpu_monitoring_instruction_sample_frequency_get
(
 void
);


void
gpu_monitoring_trace_sample_frequency_set
(
 uint32_t trace_sample_frequency
);


uint32_t
gpu_monitoring_trace_sample_frequency_get
(
 void
);



#endif
