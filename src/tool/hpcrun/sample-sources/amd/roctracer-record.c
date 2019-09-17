//
// Created by aleksa on 8/28/19.
//

#include "roctracer-record.h"
#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/sample-sources/amd/roctracer-node.h>
// include roctracer


void
roctracer_activity_produce(roctracer_record_t *activity, cct_node_t *cct_node, gpu_record_t *record, entry_data_t *data)
{
    activity_channel_t *activity_channel = &(record->activity_channel);
    activity_elem *node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
    if (!node) {
        activity_bi_unordered_channel_steal(activity_channel, channel_direction_backward);
        node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
    }
    if (!node) {
        node =(activity_elem*) hpcrun_malloc_safe(sizeof(activity_elem));
        node->node.data = NULL;
        cstack_ptr_set(&node->next, 0);
    }
    roctracer_activity_entry_set(&node->node, activity, cct_node, data);
    activity_bi_unordered_channel_push(activity_channel, channel_direction_forward, node);
}

