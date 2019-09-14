#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>

#include "cupti-channel.h"
#include "cupti-api.h"


#define channel_pop typed_bi_unordered_channel_pop(cupti_entry_correlation_t)
#define channel_push typed_bi_unordered_channel_push(cupti_entry_correlation_t)
#define channel_steal typed_bi_unordered_channel_steal(cupti_entry_correlation_t)

// a list of thread records
typedef struct channel_list_s {
  _Atomic(struct channel_list_s *) next;
  cupti_correlation_channel_t *channel;
} channel_list_t;

static _Atomic(channel_list_t *) channel_list_head;

static __thread cupti_correlation_channel_t *cupti_correlation_channel = NULL;

// implement correlation methods
typed_bi_unordered_channel_impl(cupti_entry_correlation_t)


void
cupti_correlation_channel_init()
{
  if (cupti_correlation_channel == NULL) {
    cupti_correlation_channel = hpcrun_malloc_safe(sizeof(cupti_correlation_channel_t));
    bi_unordered_channel_init(cupti_correlation_channel);

    channel_list_t *cur_head = hpcrun_malloc_safe(sizeof(channel_list_t));
    channel_list_t *old_head = atomic_load(&channel_list_head);
    atomic_store(&cur_head->next, old_head);
    cur_head->channel = cupti_correlation_channel;
    while (!atomic_compare_exchange_strong(
        &channel_list_head, &(cur_head->next), cur_head));
  }
}


cupti_correlation_channel_t *
cupti_correlation_channel_get()
{
  cupti_correlation_channel_init();
  return cupti_correlation_channel;
}


void
cupti_correlation_channel_produce
(
 cupti_correlation_channel_t *channel,
 uint64_t host_op_id,
 cupti_activity_channel_t *activity_channel,
 cct_node_t *api_node,
 cct_node_t *func_node
)
{
  cupti_correlation_channel_elem_t *node = channel_pop(channel, channel_direction_backward);
  if (!node) {
    channel_steal(channel, channel_direction_backward);
    node = channel_pop(channel, channel_direction_backward);
  }
  if (!node) {
    node = (cupti_correlation_channel_elem_t *)hpcrun_malloc_safe(sizeof(cupti_correlation_channel_elem_t));
    cstack_ptr_set(&node->next, 0);
  }
  cupti_entry_correlation_set(&node->entry, host_op_id, activity_channel, api_node, func_node);
  channel_push(channel, channel_direction_forward, node);
}


void
cupti_correlation_channel_consume(cupti_correlation_channel_t *dummy_channel)
{
  channel_list_t *head = atomic_load(&channel_list_head);
  while (head) {
    cupti_correlation_channel_t *channel = head->channel;
    cupti_correlation_channel_elem_t *node;
    channel_steal(channel, channel_direction_forward);
    while ((node = channel_pop(channel, channel_direction_forward))) {
      cupti_correlation_handle(&node->entry);
      channel_push(channel, channel_direction_backward, node);
    }
    head = atomic_load(&head->next);
  }
}
