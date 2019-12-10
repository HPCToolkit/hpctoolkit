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
 gpu_trace_item_t *ti
);


// append a synchronization event to all streams in a context
void
gpu_stream_id_map_context_process
(
 gpu_stream_id_map_entry_t **root,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
);


gpu_stream_id_map_entry_t *
gpu_stream_id_map_entry_new
(
 uint32_t stream_id
);


// signal all active streams
void
gpu_stream_map_signal_all
(
 gpu_stream_id_map_entry_t **root
);



#endif
