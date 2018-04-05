#ifndef _HPCTOOLKIT_CUPTI_FUNCTION_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_FUNCTION_ID_MAP_H_

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

typedef struct cupti_function_id_map_entry_s cupti_function_id_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_function_id_map_entry_t *cupti_function_id_map_lookup(uint64_t id);

void cupti_function_id_map_insert(uint64_t function_id, uint64_t function_index, uint64_t cubin_id);

bool cupti_function_id_map_refcnt_update(uint64_t function_id, int val);

uint64_t cupti_function_id_map_entry_refcnt_get(cupti_function_id_map_entry_t *entry);

uint64_t cupti_function_id_map_entry_function_index_get(cupti_function_id_map_entry_t *entry);

uint64_t cupti_function_id_map_entry_cubin_id_get(cupti_function_id_map_entry_t *entry);

#endif
