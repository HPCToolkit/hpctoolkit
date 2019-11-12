#ifndef gpu_host_correlation_id_map_h
#define gpu_host_correlation_id_map_h



//******************************************************************************
// global includes
//******************************************************************************

#include <stdint.h>
#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-activity-channel.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_host_correlation_map_entry_t 
gpu_host_correlation_map_entry_t; 

typedef struct cct_node_t cct_node_t; 

typedef struct gpu_op_ccts_t gpu_op_ccts_t; 



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
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_activity_channel_t *activity_channel
);


// samples == total_samples remove the node and return false
bool
gpu_host_correlation_map_samples_increase
(
 uint64_t host_correlation_id,
 int samples
);


// samples == total_samples remove the node and return false
bool
gpu_host_correlation_map_total_samples_update
(
 uint64_t host_correlation_id,
 int total_samples
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
