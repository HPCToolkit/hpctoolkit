#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>

#include "cupti-channel.h"
#include "cupti-api.h"


#define channel_pop typed_bi_unordered_channel_pop(cupti_entry_activity_t)
#define channel_push typed_bi_unordered_channel_push(cupti_entry_activity_t)
#define channel_steal typed_bi_unordered_channel_steal(cupti_entry_activity_t)

// Thread local channels
static __thread cupti_activity_channel_t *cupti_activity_channel;

// Implement activity methods
typed_bi_unordered_channel_impl(cupti_entry_activity_t)


void
cupti_activity_channel_init()
{
  if (!cupti_activity_channel) {
    cupti_activity_channel = (cupti_activity_channel_t *)hpcrun_malloc_safe(sizeof(cupti_activity_channel_t));
    bi_unordered_channel_init(cupti_activity_channel);
  }
}


cupti_activity_channel_t *
cupti_activity_channel_get()
{
  cupti_activity_channel_init();
  return cupti_activity_channel;
}


void
cupti_activity_channel_consume(cupti_activity_channel_t *channel)
{
  cupti_activity_channel_elem_t *node = NULL;
  channel_steal(channel, channel_direction_forward);
  while ((node = channel_pop(channel, channel_direction_forward))) {
    cupti_activity_handle(&node->entry);
    channel_push(channel, channel_direction_backward, node);
  }
}


void
cupti_activity_channel_produce(cupti_activity_channel_t *channel, 
  CUpti_Activity *activity, cct_node_t *cct_node)
{
  cupti_activity_channel_elem_t *node = channel_pop(channel, channel_direction_backward);
  if (!node) {
    channel_steal(channel, channel_direction_backward);
    node = channel_pop(channel, channel_direction_backward);
  }
  if (!node) {
    node = (cupti_activity_channel_elem_t *)hpcrun_malloc_safe(sizeof(cupti_activity_channel_elem_t));
    cstack_ptr_set(&node->next, 0);
  }
  cupti_entry_activity_set(&node->entry, activity, cct_node);
  channel_push(channel, channel_direction_forward, node);
}
