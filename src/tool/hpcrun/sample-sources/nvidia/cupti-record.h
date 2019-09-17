
#ifndef _HPCTOOLKIT_CUPTI_RECORD_H_
#define _HPCTOOLKIT_CUPTI_RECORD_H_

#include <hpcrun/sample-sources/gpu/gpu-record.h>
#include <cupti_activity.h>

void
cupti_activity_produce
(
        CUpti_Activity *activity,
        cct_node_t *cct_node,
        gpu_record_t *record
);

#endif

