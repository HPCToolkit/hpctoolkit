#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>
#include "cupti-stack.h"
#include "cupti-record.h"
#include "cupti-api.h"

static _Atomic(cupti_record_list_t *) cupti_record_list_head;

static __thread cupti_record_t cupti_record;

static __thread bool cupti_record_initialized = false;

typed_bi_unordered_channel_impl(cupti_entry_correlation_t)
typed_bi_unordered_channel_impl(cupti_entry_activity_t)

void
cupti_record_init()
{
  if (!cupti_record_initialized) {
    bi_unordered_channel_init(cupti_record.correlation_channel);
    bi_unordered_channel_init(cupti_record.activity_channel);
    cupti_record_initialized = true;

    cupti_record_list_t *curr_cupti_record_list_head = hpcrun_malloc_safe(sizeof(cupti_record_list_t));
    cupti_record_list_t *old_head = atomic_load(&cupti_record_list_head);
    atomic_store(&curr_cupti_record_list_head->next, old_head);
    curr_cupti_record_list_head->record = &cupti_record;
    while (!atomic_compare_exchange_strong(
      &cupti_record_list_head, &(curr_cupti_record_list_head->next), curr_cupti_record_list_head));
  }
}

cupti_record_t *
cupti_record_get()
{
  cupti_record_init();
  return &cupti_record;
}

//correlation produce
void
correlation_produce(uint64_t host_op_id, cct_node_t *cct_node)
{

  correlation_channel_t *correlation_channel = &(cupti_record.correlation_channel);
  cupti_correlation_elem *node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_backward);
  if (!node) {
      correlation_bi_unordered_channel_steal(correlation_channel, channel_direction_backward);
      node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_backward);
  }
    if (!node) {
        node =(cupti_correlation_elem* ) hpcrun_malloc_safe(sizeof(cupti_correlation_elem));
        cstack_ptr_set(&node->next, 0);
    }
  cupti_correlation_entry_set(&node->node, host_op_id, cct_node, &cupti_record);
  correlation_bi_unordered_channel_push(correlation_channel, channel_direction_forward, node);
}

void
cupti_activities_consume()
{
  activity_channel_t *activity_channel = &(cupti_record.activity_channel);
  cupti_activity_elem *node;
  activity_bi_unordered_channel_steal(activity_channel, channel_direction_forward);
  while ((node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_forward))) {
      cupti_activity_handle(&node->node);
      activity_bi_unordered_channel_push(activity_channel, channel_direction_backward, node);
  }
}
//cor consume
void
correlations_consume()
{
  cupti_record_list_t *head = atomic_load(&cupti_record_list_head);
  while (head) {
      correlation_channel_t *correlation_channel = &(head->record->correlation_channel);
      cupti_correlation_elem *node;
      correlation_bi_unordered_channel_steal(correlation_channel, channel_direction_forward);
      while ((node = correlation_bi_unordered_channel_pop(correlation_channel, channel_direction_forward))) {
          cupti_correlation_handle(&node->node);
          correlation_bi_unordered_channel_push(correlation_channel, channel_direction_backward, node);
      }
      head = atomic_load(&head->next);
  }
}

void
cupti_activity_produce(CUpti_Activity *activity, cct_node_t *cct_node, cupti_record_t *record)
{
  activity_channel_t *activity_channel = &(record->activity_channel);
  cupti_activity_elem *node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
  if (!node) {
      activity_bi_unordered_channel_steal(activity_channel, channel_direction_backward);
      node = activity_bi_unordered_channel_pop(activity_channel, channel_direction_backward);
  }
  if (!node) {
      node =(cupti_activity_elem*) hpcrun_malloc_safe(sizeof(cupti_activity_elem));
      cstack_ptr_set(&node->next, 0);
  }
  cupti_activity_entry_set(&node->node, activity, cct_node);
  activity_bi_unordered_channel_push(activity_channel, channel_direction_forward, node);
}
