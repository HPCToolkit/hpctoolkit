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
  uint64_t globalMemoryBandwidth;
  uint64_t globalMemorySize;
  uint32_t constantMemorySize;
  uint32_t l2CacheSize;
  uint32_t numThreadsPerWarp;
  uint32_t coreClockRate;
  uint32_t numMemcpyEngines;
  uint32_t numMultiprocessors;
  uint32_t maxIPC;
  uint32_t maxWarpsPerMultiprocessor;
  uint32_t maxBlocksPerMultiprocessor;
  uint32_t maxSharedMemoryPerMultiprocessor;
  uint32_t maxRegistersPerMultiprocessor;
  uint32_t maxRegistersPerBlock;
  uint32_t maxSharedMemoryPerBlock;
  uint32_t maxThreadsPerBlock;
  uint32_t maxBlockDimX;
  uint32_t maxBlockDimY;
  uint32_t maxBlockDimZ;
  uint32_t maxGridDimX;
  uint32_t maxGridDimY;
  uint32_t maxGridDimZ;
  uint32_t computeCapabilityMajor;
  uint32_t computeCapabilityMinor;
  uint32_t eccEnabled;
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
 uint32_t device_id,
 CUpti_ActivityDevice2 *device
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
