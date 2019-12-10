//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-channel-item-allocator.h"
#include "gpu-trace-item.h"



//******************************************************************************
// interface operations 
//******************************************************************************

void
gpu_trace_item_produce
(
 gpu_trace_item_t *ti,
 uint64_t cpu_submit_time,
 uint64_t start,
 uint64_t end,
 cct_node_t *call_path_leaf
)
{
  ti->cpu_submit_time = cpu_submit_time;
  ti->start = start;
  ti->end = end;
  ti->call_path_leaf = call_path_leaf;
  cstack_ptr_set(&(ti->next), 0);
}


void
gpu_trace_item_consume
(
 gpu_trace_item_consume_fn_t trace_item_consume,
 thread_data_t *td,
 gpu_trace_item_t *ti
)
{
  trace_item_consume(td, ti->call_path_leaf, ti->start, ti->end);
}


gpu_trace_item_t *
gpu_trace_item_alloc
(
 gpu_trace_channel_t *channel
)
{
  return channel_item_alloc(channel, gpu_trace_item_t);
}


void
gpu_trace_item_free
(
 gpu_trace_channel_t *channel, 
 gpu_trace_item_t *ti
)
{
  channel_item_free(channel, ti);
}

