#ifndef _HPCTOOLKIT_NVIDIA_H_
#define _HPCTOOLKIT_NVIDIA_H_

#include <hpcrun/cct/cct.h>
#include <cupti.h>

extern void cupti_activity_attribute(CUpti_Activity *record, cct_node_t *node);

extern int cupti_pc_sampling_frequency_get();

#endif
