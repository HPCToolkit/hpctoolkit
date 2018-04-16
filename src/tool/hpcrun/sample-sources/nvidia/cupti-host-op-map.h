#ifndef _HPCTOOLKIT_CUPTI_HOST_OP_MAP_H_
#define _HPCTOOLKIT_CUPTI_HOST_OP_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>

#include "cupti-record.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_host_op_map_entry_s cupti_host_op_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_host_op_map_entry_t *cupti_host_op_map_lookup(uint64_t id);

void cupti_host_op_map_insert(uint64_t host_op_id,
                              cct_node_t *host_op_node,
                              cupti_record_t *record);

// samples == total_samples remove the node and return false
bool cupti_host_op_map_samples_increase(uint64_t host_op_id, int samples);

// samples == total_samples remove the node and return false
bool cupti_host_op_map_total_samples_update(uint64_t host_op_id, int total_samples);

void cupti_host_op_map_delete(uint64_t host_op_id);

cct_node_t *cupti_host_op_map_entry_host_op_node_get(cupti_host_op_map_entry_t *entry);

cupti_record_t *cupti_host_op_map_entry_record_get(cupti_host_op_map_entry_t *entry);

#endif

