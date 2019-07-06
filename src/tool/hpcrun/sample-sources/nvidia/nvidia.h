#ifndef _HPCTOOLKIT_NVIDIA_H_
#define _HPCTOOLKIT_NVIDIA_H_

#include <hpcrun/cct/cct.h>
#include <cupti.h>
#include "cupti-node.h"
#include "sanitizer-node.h"

extern void cupti_activity_attribute(cupti_activity_t *activity, cct_node_t *cct_node);

extern void sanitizer_activity_attribute(sanitizer_activity_t *activity, cct_node_t *cct_node);

extern int cupti_pc_sampling_frequency_get();

extern int sanitizer_block_sampling_frequency_get();

extern void cupti_enable_activities(CUcontext context);

#endif
