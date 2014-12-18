#ifndef _hpctoolkit_ompt_region_map_h_
#define _hpctoolkit_ompt_region_map_h_

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

typedef struct ompt_region_map_entry_s ompt_region_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_region_map_entry_t *ompt_region_map_lookup(uint64_t id);

void ompt_region_map_insert(uint64_t region_id, cct_node_t *call_path);

bool ompt_region_map_refcnt_update(uint64_t region_id, int val);

uint64_t ompt_region_map_entry_refcnt_get(ompt_region_map_entry_t *node);

void ompt_region_map_entry_callpath_set(ompt_region_map_entry_t *node, 
				       cct_node_t *call_path);

cct_node_t *ompt_region_map_entry_callpath_get(ompt_region_map_entry_t *node);

#endif
