// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_trace_item_h
#define gpu_trace_item_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_item_t {
  uint64_t cpu_submit_time;
  uint64_t start;
  uint64_t end;
  cct_node_t *call_path_leaf;
} gpu_trace_item_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * Initializes \p trace_item
 * \param call_path_leaf cannot be NULL
*/
void
gpu_trace_item_init
(
 gpu_trace_item_t *trace_item,
 uint64_t cpu_submit_time,
 uint64_t start,
 uint64_t end,
 cct_node_t *call_path_leaf
);


/**
 * Logs \p context and \p trace_item
*/
void
gpu_trace_item_dump
(
  const gpu_trace_item_t *trace_item,
  const char *context
);



#endif
