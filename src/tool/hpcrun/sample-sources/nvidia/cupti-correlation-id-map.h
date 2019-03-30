#ifndef _HPCTOOLKIT_CUPTI_CORRELATION_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_CORRELATION_ID_MAP_H_

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

typedef struct cupti_correlation_id_map_entry_s cupti_correlation_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_correlation_id_map_entry_t *
cupti_correlation_id_map_lookup
(
 uint32_t id
);


void
cupti_correlation_id_map_insert
(
 uint32_t correlation_id,
 uint64_t external_id
);


void
cupti_correlation_id_map_delete
(
 uint32_t correlation_id
);


void
cupti_correlation_id_map_external_id_replace
(
 uint32_t correlation_id,
 uint64_t external_id
);


void
cupti_correlation_id_map_kernel_update
(
 uint32_t correlation_id,
 uint32_t device_id,
 uint64_t start,
 uint64_t end
);


uint64_t
cupti_correlation_id_map_entry_external_id_get
(
 cupti_correlation_id_map_entry_t *entry
);


uint64_t
cupti_correlation_id_map_entry_start_get
(
 cupti_correlation_id_map_entry_t *entry
);


uint64_t
cupti_correlation_id_map_entry_end_get
(
 cupti_correlation_id_map_entry_t *entry
);


uint32_t
cupti_correlation_id_map_entry_device_id_get
(
 cupti_correlation_id_map_entry_t *entry
);

#endif
