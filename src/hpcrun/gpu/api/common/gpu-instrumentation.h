// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_instrumentation_h
#define gpu_instrumentation_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct {
  bool count_instructions;
  bool analyze_simd;
  bool attribute_latency;
  bool silent; // no warnings to stderr from a binary instrumentation tool
} gpu_instrumentation_t;


void
gpu_instrumentation_options_set
(
 const char *str,
 const char *prefix,
 gpu_instrumentation_t *options
);


bool
gpu_instrumentation_enabled
(
 gpu_instrumentation_t *o
);



#endif
