#ifndef _HPCTOOLKIT_CUPTI_DEVICE_ID_MAP_H_
#define _HPCTOOLKIT_CUPTI_DEVICE_ID_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>
#include <cupti.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct cupti_device_id_map_entry_s cupti_device_id_map_entry_t;

typedef struct cupti_device_property {
  uint32_t sm_count;
  uint32_t sm_clock_rate;
  uint32_t sm_shared_memory;
  uint32_t sm_registers;
  uint32_t sm_threads;
  uint32_t sm_blocks;
  uint32_t num_threads_per_warp;
} cupti_device_property_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

cupti_device_id_map_entry_t *
cupti_device_id_map_lookup
(
 uint32_t id
);


void
cupti_device_id_map_insert
(
 uint32_t device_id
);


void
cupti_device_id_map_delete
(
 uint32_t device_id
);


cupti_device_property_t *
cupti_device_id_map_entry_device_property_get
(
 cupti_device_id_map_entry_t *entry
);

#endif
