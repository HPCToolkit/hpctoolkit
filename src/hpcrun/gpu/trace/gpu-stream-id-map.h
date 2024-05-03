// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_stream_id_map_h
#define gpu_stream_id_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../common/lean/splay-uint64.h"

#include "gpu-trace-channel.h"



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct gpu_stream_id_map_entry_t gpu_stream_id_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_stream_id_map_entry_t *
gpu_stream_id_map_lookup
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id
);


void
gpu_stream_id_map_delete
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id
);


_Bool
gpu_stream_id_map_insert_entry
(
  gpu_stream_id_map_entry_t **root,
  gpu_stream_id_map_entry_t *entry
);

void
gpu_stream_id_map_for_each
(
 gpu_stream_id_map_entry_t **root,
 void (*iter_fn)(gpu_trace_channel_t *, void *),
 void *arg
);



//*****************************************************************************
// entry interface operations
//*****************************************************************************

gpu_stream_id_map_entry_t *
gpu_stream_id_map_entry_new
(
 uint32_t stream_id
);


void
gpu_stream_id_map_entry_set_channel
(
  gpu_stream_id_map_entry_t *entry,
  gpu_trace_channel_t *channel
);


gpu_trace_channel_t *
gpu_stream_id_map_entry_get_channel
(
  gpu_stream_id_map_entry_t *entry
);

#endif
