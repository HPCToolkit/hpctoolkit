//
// Created by user on 17.8.2019..
//

#ifndef HPCTOOLKIT_ROCTRACER_NODE_H
#define HPCTOOLKIT_ROCTRACER_NODE_H

#include <hpcrun/sample-sources/gpu/gpu-record.h>
#include <roctracer_hip.h>
#include <roctracer_hcc.h>



void
roctracer_activity_entry_set
(
        entry_activity_t *entry,
        roctracer_record_t *activity,
        cct_node_t *cct_node,
        entry_data_t *data
);

#endif //HPCTOOLKIT_ROCTRACER_NODE_H
