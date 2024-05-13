// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef gpu_papi_kernel_map_h_
#define gpu_papi_kernel_map_h_

//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../../cct/cct.h"                               // cct_node_t
#include "../../../../activity/gpu-activity-channel.h"     // gpu_activity_channel_t



//******************************************************************************
// type declarations
//******************************************************************************

// Each kernel_node_t maintains information about an asynchronous kernel activity
typedef struct papi_kernel_node_t {
  // we save the id for the kernel inside the node so that ids of processed kernel nodes can be returned on sync_epilogue
  uint64_t kernel_id;
  // CCT node of the CPU thread that launched this activity
  cct_node_t *cct;
  gpu_activity_channel_t *activity_channel;
  long long prev_values[2];
  struct papi_kernel_node_t* next;
} papi_kernel_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

typedef struct papi_kernel_map_entry_t papi_kernel_map_entry_t;

papi_kernel_map_entry_t*
papi_kernel_map_lookup
(
 uint64_t kernel_id
);


void
papi_kernel_map_insert
(
 uint64_t kernel_id,
 papi_kernel_node_t *kernel_node
);


void
papi_kernel_map_delete
(
 uint64_t kernel_id
);


papi_kernel_node_t*
papi_kernel_map_entry_kernel_node_get
(
 papi_kernel_map_entry_t *entry
);

#endif          // gpu_papi_kernel_map_h_
