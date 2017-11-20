#ifndef _hpctoolkit_ompt_function_id_map_h_
#define _hpctoolkit_ompt_function_id_map_h_

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

typedef struct ompt_function_id_map_entry_s ompt_function_id_map_entry_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

ompt_function_id_map_entry_t *ompt_function_id_map_lookup(uint64_t id);

void ompt_function_id_map_insert(uint64_t function_id, uint64_t function_index, uint64_t cubin_id);

bool ompt_function_id_map_refcnt_update(uint64_t function_id, int val);

uint64_t ompt_function_id_map_entry_refcnt_get(ompt_function_id_map_entry_t *entry);

uint64_t ompt_function_id_map_entry_function_index_get(ompt_function_id_map_entry_t *entry);

uint64_t ompt_function_id_map_entry_cubin_id_get(ompt_function_id_map_entry_t *entry);

#endif
