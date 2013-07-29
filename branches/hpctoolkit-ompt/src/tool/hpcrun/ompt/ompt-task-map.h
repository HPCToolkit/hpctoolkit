#ifndef _hpctoolkit_ompt_task_map_h_
#define _hpctoolkit_ompt_task_map_h_

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

typedef struct ompt_task_map_entry_s ompt_task_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_task_map_entry_t *ompt_task_map_lookup(uint64_t id);

void ompt_task_map_insert(uint64_t task_id, cct_node_t *call_path);

void ompt_task_map_entry_callpath_set(ompt_task_map_entry_t *node, 
				       cct_node_t *call_path);

cct_node_t *ompt_task_map_entry_callpath_get(ompt_task_map_entry_t *node);

#endif
