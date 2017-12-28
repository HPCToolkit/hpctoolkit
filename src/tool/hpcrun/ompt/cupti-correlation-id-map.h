#ifndef _hpctoolkit_cupti_correlation_id_map_h_
#define _hpctoolkit_cupti_correlation_id_map_h_

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

typedef struct ompt_correlation_id_map_entry_s ompt_correlation_id_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_correlation_id_map_entry_t *ompt_correlation_id_map_lookup(uint64_t id);

void ompt_correlation_id_map_insert(uint64_t correlation_id, uint64_t external_id);

bool ompt_correlation_id_map_refcnt_update(uint64_t correlation_id, int val);

uint64_t ompt_correlation_id_map_entry_refcnt_get(ompt_correlation_id_map_entry_t *entry);

uint64_t ompt_correlation_id_map_entry_external_id_get(ompt_correlation_id_map_entry_t *entry);

#endif

