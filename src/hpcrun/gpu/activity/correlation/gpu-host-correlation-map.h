// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_host_correlation_id_map_h
#define gpu_host_correlation_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
#include <stdbool.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_host_correlation_map_entry_t
gpu_host_correlation_map_entry_t;

typedef struct cct_node_t cct_node_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

gpu_host_correlation_map_entry_t *
gpu_host_correlation_map_lookup
(
 uint64_t host_correlation_id
);


void
gpu_host_correlation_map_insert
(
 uint64_t host_correlation_id,
 gpu_activity_channel_t *activity_channel
);


void
gpu_host_correlation_map_delete
(
 uint64_t host_correlation_id
);


//------------------------------------------------------------------------------
// accessor methods
//------------------------------------------------------------------------------

gpu_activity_channel_t *
gpu_host_correlation_map_entry_channel_get
(
 gpu_host_correlation_map_entry_t *entry
);


#endif
