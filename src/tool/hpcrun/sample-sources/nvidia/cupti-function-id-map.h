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
#include <hpcrun/utilities/ip-normalized.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_function_id_map_entry_s cupti_function_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_function_id_map_entry_t *
cupti_function_id_map_lookup
(
 uint32_t function_id
);


void
cupti_function_id_map_insert
(
 uint32_t function_id,
 ip_normalized_t ip_norm
);


ip_normalized_t
cupti_function_id_map_entry_ip_norm_get
(
 cupti_function_id_map_entry_t *entry
);

#endif
