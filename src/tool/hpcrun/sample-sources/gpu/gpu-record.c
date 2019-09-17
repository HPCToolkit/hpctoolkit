#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "gpu-record.h"

#if GPU_RECORD_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



static _Atomic(gpu_record_list_t *) gpu_record_list_head;

static __thread gpu_record_t gpu_record;

static __thread bool gpu_record_initialized = false;

typed_bi_unordered_channel_impl(entry_correlation_t)
typed_bi_unordered_channel_impl(entry_activity_t)


static void
correlation_entry_set
(
    entry_correlation_t *entry,
    uint64_t host_op_id,
    cct_node_t *cct_node,
    void *record,
    entry_data_t *entry_data
)
{
   entry->host_op_id = host_op_id;
   entry->cct_node = cct_node;
   entry->record = record;
   entry->data = entry_data;
}


static void
correlation_handle
(
    entry_correlation_t *entry
)
{
    PRINT("Insert external id %d\n", entry->host_op_id);
    gpu_host_op_map_insert
            (entry->host_op_id, entry->cct_node,
             entry->record, entry->data);
}


void
gpu_record_init()
{
    if (!gpu_record_initialized) {
        bi_unordered_channel_init(gpu_record.correlation_channel);
        bi_unordered_channel_init(gpu_record.activity_channel);
        gpu_record_initialized = true;

        gpu_record_list_t *curr_gpu_record_list_head = hpcrun_malloc_safe(sizeof(gpu_record_list_t));
        gpu_record_list_t *old_head = atomic_load(&gpu_record_list_head);
        atomic_store(&curr_gpu_record_list_head->next, old_head);
        curr_gpu_record_list_head->record = &gpu_record;
        while (!atomic_compare_exchange_strong(
                &gpu_record_list_head, &(curr_gpu_record_list_head->next), curr_gpu_record_list_head));
    }
}

gpu_record_t *
gpu_record_get()
{
    gpu_record_init();
    return &gpu_record;
}

//correlation produce
void
correlation_produce(uint64_t host_op_id, cct_node_t *cct_node, entry_data_t *entry_data)
{

    correlation_channel_t *correlation_channel = &(gpu_record.correlation_channel);
    correlation_elem *node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_backward);
    if (!node) {
        correlation_bi_unordered_channel_steal(correlation_channel, channel_direction_backward);
        node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_backward);
    }
    if (!node) {
        node =(correlation_elem* ) hpcrun_malloc_safe(sizeof(correlation_elem));
        cstack_ptr_set(&node->next, 0);
    }
    correlation_entry_set(&node->node, host_op_id, cct_node, &gpu_record, entry_data);
    correlation_bi_unordered_channel_push(correlation_channel, channel_direction_forward, node);
}

void
activities_consume(gpu_fn_t fn)
{
    activity_channel_t *activity_channel = &(gpu_record.activity_channel);
    activity_elem *node;
    activity_bi_unordered_channel_steal(activity_channel, channel_direction_forward);
    while ((node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_forward))) {
        fn(&node->node);
        activity_bi_unordered_channel_push(activity_channel, channel_direction_backward, node);
    }
}
//cor consume
void
correlations_consume()
{
    gpu_record_list_t *head = atomic_load(&gpu_record_list_head);
    while (head) {
        correlation_channel_t *correlation_channel = &(head->record->correlation_channel);
        correlation_elem *node;
        correlation_bi_unordered_channel_steal(correlation_channel, channel_direction_forward);
        while ((node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_forward))) {
            correlation_handle(&node->node);
            //cupti_correlation_handle(&node->node);
            correlation_bi_unordered_channel_push(correlation_channel, channel_direction_backward, node);
        }
        head = atomic_load(&head->next);
    }
}






