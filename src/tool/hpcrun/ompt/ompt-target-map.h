#ifndef _hpctoolkit_ompt_target_map_h_
#define _hpctoolkit_ompt_target_map_h_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct-node-vector.h>


/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct ompt_target_map_entry_s ompt_target_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_target_map_entry_t *ompt_target_map_lookup(cct_node_t *target);

cct_node_t *ompt_target_map_seq_lookup(cct_node_t *target, uint64_t id);

void ompt_target_map_child_insert(cct_node_t *target, cct_node_t *cct_node);

void ompt_target_map_insert(cct_node_t *target);

bool ompt_target_map_refcnt_update(cct_node_t *target, int val);

uint64_t ompt_target_map_entry_refcnt_get(ompt_target_map_entry_t *entry);


#endif
