// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_context_id_map_h
#define gpu_context_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-stream-id-map.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_context_id_map_entry_t gpu_context_id_map_entry_t;



//******************************************************************************
// interface declarations
//******************************************************************************

gpu_context_id_map_entry_t *
gpu_context_id_map_lookup
(
 uint32_t context_id
);


gpu_context_id_map_entry_t *
gpu_context_id_map_entry_new
(
 uint32_t context_id
);


_Bool
gpu_context_id_map_insert_entry
(
 gpu_context_id_map_entry_t *entry
);


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
);


gpu_stream_id_map_entry_t **
gpu_context_id_map_entry_get_streams
(
  gpu_context_id_map_entry_t *entry
);


void
gpu_context_id_map_set_cpu_gpu_timestamp
(
  uint64_t t1,
  uint64_t t2
);


void
gpu_context_id_map_adjust_times
(
 gpu_context_id_map_entry_t *entry,
 gpu_trace_item_t *ti
);


#endif
