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
#include <hpcrun/ompt/ompt-region-map.h>


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct ompt_host_op_map_entry_s ompt_host_op_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_host_op_map_entry_t *ompt_host_op_map_lookup(uint64_t id);

void ompt_host_op_map_insert(uint64_t host_op_id,
                             uint64_t host_op_seq_id,
                             ompt_region_map_entry_t *map_entry);

bool ompt_host_op_map_refcnt_update(uint64_t host_op_id, int val);

uint64_t ompt_host_op_map_entry_refcnt_get(ompt_host_op_map_entry_t *entry);

ompt_region_map_entry_t *ompt_host_op_map_entry_region_map_entry_get(ompt_host_op_map_entry_t *entry);

uint64_t ompt_host_op_map_entry_seq_id_get(ompt_host_op_map_entry_t *entry);

#endif

