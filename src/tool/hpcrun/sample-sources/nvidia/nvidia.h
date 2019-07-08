#ifndef _HPCTOOLKIT_NVIDIA_H_
#define _HPCTOOLKIT_NVIDIA_H_

#include <hpcrun/cct/cct.h>
#include <cupti.h>
#include "cupti-node.h"

void cupti_activity_attribute(cupti_activity_t *activity, cct_node_t *cct_node);

int cupti_pc_sampling_frequency_get();

void cupti_enable_activities(CUcontext context);

#endif
