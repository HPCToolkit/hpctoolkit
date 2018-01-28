#ifndef _HPCTOOLKIT_NVIDIA_H_
#define _HPCTOOLKIT_NVIDIA_H_

#include <hpcrun/cct/cct.h>
#include <cupti.h>

extern void cupti_attribute_activity(CUpti_Activity *record, cct_node_t *node);

#endif
