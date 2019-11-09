#ifndef gpu_function_id_map_h
#define gpu_function_id_map_h



//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/utilities/ip-normalized.h>



//*****************************************************************************
// forward declarations
//*****************************************************************************

typedef struct cct_node_t cct_node_t;



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct gpu_function_id_map_entry_t gpu_function_id_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_function_id_map_entry_t *
gpu_function_id_map_lookup
(
 uint32_t function_id
);


void
gpu_function_id_map_insert
(
 uint32_t function_id,
 ip_normalized_t ip_norm
);


ip_normalized_t
gpu_function_id_map_entry_ip_norm_get
(
 gpu_function_id_map_entry_t *entry
);



#endif
