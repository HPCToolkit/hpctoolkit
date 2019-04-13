#ifndef _HPCTOOLKIT_CUDA_API_H_
#define _HPCTOOLKIT_CUDA_API_H_

#include "cupti-device-id-map.h"

extern void cuda_device_property_query(int device_id, cupti_device_property_t *property);

extern int cuda_device_sm_blocks_query(int major, int minor);

#endif
