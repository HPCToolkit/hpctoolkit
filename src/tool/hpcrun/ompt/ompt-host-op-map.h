#ifndef _hpctoolkit_ompt_host_op_map_h_
#define _hpctoolkit_ompt_host_op_map_h_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>



/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct ompt_host_op_map_entry_s ompt_host_op_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_host_op_map_entry_t *ompt_host_op_map_lookup(uint64_t id);

void ompt_host_op_map_insert(uint64_t host_op_id, cct_node_t *call_path);

bool ompt_host_op_map_refcnt_update(uint64_t host_op_id, int val);

uint64_t ompt_host_op_map_entry_refcnt_get(ompt_host_op_map_entry_t *node);

void ompt_host_op_map_entry_callpath_set(ompt_host_op_map_entry_t *node,
                                         cct_node_t *call_path);

cct_node_t *ompt_host_op_map_entry_callpath_get(ompt_host_op_map_entry_t *node);

#endif

