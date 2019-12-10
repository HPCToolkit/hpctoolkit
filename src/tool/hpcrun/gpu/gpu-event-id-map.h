#ifndef gpu_event_id_map_h
#define gpu_event_id_map_h



//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/splay-uint64.h>



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct gpu_event_id_map_entry_t gpu_event_id_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_event_id_map_entry_t *
gpu_event_id_map_lookup
(
 uint32_t event_id
);


void
gpu_event_id_map_insert
(
 uint32_t event_id,
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_event_id_map_delete
(
 uint32_t event_id
);


uint32_t
gpu_event_id_map_entry_stream_id_get
(
 gpu_event_id_map_entry_t *entry
);


uint32_t
gpu_event_id_map_entry_context_id_get
(
 gpu_event_id_map_entry_t *entry
);


#endif

