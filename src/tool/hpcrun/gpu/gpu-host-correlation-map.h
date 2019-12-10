#ifndef gpu_host_correlation_id_map_h
#define gpu_host_correlation_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-op-placeholders.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_host_correlation_map_entry_t 
gpu_host_correlation_map_entry_t; 

typedef struct cct_node_t cct_node_t; 

typedef struct gpu_activity_channel_t gpu_activity_channel_t;

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
 uint64_t cpu_gpu_time_offset,
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


cct_node_t *
gpu_host_correlation_map_entry_op_cct_get
(
 gpu_host_correlation_map_entry_t *entry,
 gpu_placeholder_type_t ph_type
);


cct_node_t *
gpu_host_correlation_map_entry_op_function_get
(
 gpu_host_correlation_map_entry_t *entry
);


uint64_t
gpu_host_correlation_map_entry_cpu_submit_time
(
 gpu_host_correlation_map_entry_t *entry
);



#endif
