//
// Created by aleksa on 8/28/19.
//

#ifndef HPCTOOLKIT_ROCTRACER_RECORD_H
#define HPCTOOLKIT_ROCTRACER_RECORD_H

#include <hpcrun/sample-sources/gpu/gpu-record.h>
#include <roctracer_hip.h>

void
roctracer_activity_produce
(
    roctracer_record_t *activity,
    cct_node_t *cct_node,
    gpu_record_t *record,
    entry_data_t *data
);

#endif //HPCTOOLKIT_ROCTRACER_RECORD_H
