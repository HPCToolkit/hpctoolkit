#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>

#include <assert.h>

#include "cupti-channel.h"
#include "cupti-trace-api.h"


#define channel_pop typed_bi_unordered_channel_pop(cupti_entry_trace_t)
#define channel_push typed_bi_unordered_channel_push(cupti_entry_trace_t)
#define channel_steal typed_bi_unordered_channel_steal(cupti_entry_trace_t)

// thread local channels
static __thread cupti_trace_channel_t *cupti_trace_channel;

// implement trace methods
typed_bi_unordered_channel_impl(cupti_entry_trace_t)


// Do not use
void
cupti_trace_channel_init()
{
  if (!cupti_trace_channel) {
    cupti_trace_channel = (cupti_trace_channel_t *)hpcrun_malloc_safe(sizeof(cupti_trace_channel_t));
    bi_unordered_channel_init(cupti_trace_channel);
  }
}


// Do not use
cupti_trace_channel_t *
cupti_trace_channel_get()
{
  cupti_trace_channel_init();
  return cupti_trace_channel;
}


void
cupti_trace_channel_consume(cupti_trace_channel_t *channel)
{
  cupti_trace_channel_elem_t *node = NULL;
  channel_steal(channel, channel_direction_forward);
  // Reverse the entire chain
  sstack_reverse(&(channel->forward_stack.to_consume));
  while ((node = channel_pop(channel, channel_direction_forward))) {
    cupti_trace_handle(&node->entry);
    channel_push(channel, channel_direction_backward, node);
  }
}


void
cupti_trace_channel_produce(cupti_trace_channel_t *channel, 
  uint64_t start, uint64_t end, cct_node_t *cct_node)
{
  cupti_trace_channel_elem_t *node = channel_pop(channel, channel_direction_backward);
  if (!node) {
    channel_steal(channel, channel_direction_backward);
    node = channel_pop(channel, channel_direction_backward);
  }
  if (!node) {
    node = (cupti_trace_channel_elem_t *)hpcrun_malloc_safe(sizeof(cupti_trace_channel_elem_t));
    cstack_ptr_set(&node->next, 0);
  }
  cupti_entry_trace_set(&node->entry, start, end, cct_node);
  channel_push(channel, channel_direction_forward, node);
}
