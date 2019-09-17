#include "cupti-record.h"
#include <cupti_version.h>
#include "cupti-node.h"
#include <hpcrun/memory/hpcrun-malloc.h>


void
cupti_activity_produce(CUpti_Activity *activity, cct_node_t *cct_node, gpu_record_t *record)
{
    activity_channel_t *activity_channel = &(record->activity_channel);
    activity_elem *node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
    if (!node) {
        activity_bi_unordered_channel_steal(activity_channel, channel_direction_backward);
        node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
    }
    if (!node) {
        node =(activity_elem*) hpcrun_malloc_safe(sizeof(activity_elem));
        cstack_ptr_set(&node->next, 0);
    }
    cupti_activity_entry_set(&node->node, activity, cct_node);
    activity_bi_unordered_channel_push(activity_channel, channel_direction_forward, node);
}