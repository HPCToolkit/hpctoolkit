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

#include "cupti-activity-queue.h"


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_host_op_map_entry_s cupti_host_op_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_host_op_map_entry_t *cupti_host_op_map_lookup(uint64_t id);

void cupti_host_op_map_insert(uint64_t host_op_id,
                              uint64_t host_op_seq_id,
                              cct_node_t *target_node,
                              cct_node_t *host_op_node);

bool cupti_host_op_map_refcnt_update(uint64_t host_op_id, int val);

uint64_t cupti_host_op_map_entry_refcnt_get(cupti_host_op_map_entry_t *entry);

cct_node_t *cupti_host_op_map_entry_target_node_get(cupti_host_op_map_entry_t *entry);

cct_node_t *cupti_host_op_map_entry_host_op_node_get(cupti_host_op_map_entry_t *entry);

uint64_t cupti_host_op_map_entry_seq_id_get(cupti_host_op_map_entry_t *entry);

cupti_activity_queue_t *cupti_host_op_map_entry_activity_queue_get(cupti_host_op_map_entry_t *entry);

#endif

