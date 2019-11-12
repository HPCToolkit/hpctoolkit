#ifndef gpu_context_id_map_h
#define gpu_context_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-channel.h"
#include "gpu-trace.h"
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


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
);


void
gpu_context_id_map_stream_delete
(
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_context_id_map_stream_process
(
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_fn_t fn,
 void *arg
);


void
gpu_context_id_map_context_process
(
 uint32_t context_id,
 gpu_trace_fn_t fn,
 void *arg
);


void
gpu_context_id_map_device_process
(
 gpu_trace_fn_t fn,
 void *arg
);

#endif

