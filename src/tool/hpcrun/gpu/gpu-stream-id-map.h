#ifndef gpu_stream_id_map_h
#define gpu_stream_id_map_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>

#include "gpu-trace-channel.h"



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct gpu_stream_id_map_entry_t gpu_stream_id_map_entry_t;

typedef void (*gpu_trace_fn_t)
(
 gpu_trace_channel_t *channel, 
 void *arg
);



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


void
gpu_stream_id_map_stream_process
(
 gpu_stream_id_map_entry_t **root,
 uint32_t stream_id,
 gpu_trace_fn_t fn,
 void *arg
);


// Two usage cases:
// 1. Append events to all active streams (cuContextSynchronize)
// 2. Signal all active streams
void
gpu_stream_id_map_context_process
(
 gpu_stream_id_map_entry_t **root,
 gpu_trace_fn_t fn,
 void *arg
);


gpu_stream_id_map_entry_t *
gpu_stream_id_map_entry_new
(
 uint32_t stream_id
);

#endif
