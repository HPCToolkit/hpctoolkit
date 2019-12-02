//
// Created by user on 17.8.2019..
//

#ifndef HPCTOOLKIT_ROCTRACER_NODE_H
#define HPCTOOLKIT_ROCTRACER_NODE_H

#include <hpcrun/gpu/gpu-activity.h>
#include <roctracer_hip.h>


void
roctracer_activity_translate
(
 gpu_activity_t *entry,
 roctracer_record_t *record,
 cct_node_t *cct_node        
);

#endif //HPCTOOLKIT_ROCTRACER_NODE_H
