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
  int sm_count;
  int sm_clock_rate;
  int sm_shared_memory;
  int sm_registers;
  int sm_threads;
  int sm_blocks;
  int num_threads_per_warp;
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
