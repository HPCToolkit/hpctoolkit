//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/sample-sources/nvidia.h>

#include "gpu-activity.h"
#include "gpu-channel-item-allocator.h"



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_context_activity_dump
(
 gpu_activity_t *activity,
 const char *context
)
{
  PRINT("context %s gpu activity %p kind = %d\n", context, activity, activity->kind);
}


void
gpu_activity_dump
(
 gpu_activity_t *activity
)
{
  gpu_context_activity_dump(activity, "DEBUGGER");
}


void
gpu_activity_consume
(
 gpu_activity_t *activity,
 gpu_activity_attribute_fn_t aa_fn
)
{
  gpu_context_activity_dump(activity, "CONSUME");

  aa_fn(activity);
}


gpu_activity_t *
gpu_activity_alloc
(
 gpu_activity_channel_t *channel
)
{
  return channel_item_alloc(channel, gpu_activity_t);
}


void
gpu_activity_free
(
 gpu_activity_channel_t *channel, 
 gpu_activity_t *a
)
{
  channel_item_free(channel, a);
}

void
set_gpu_instruction
(
  gpu_instruction_t* insn, 
  ip_normalized_t pc
)
{
  insn->pc = pc;
}

void
set_gpu_interval
(
  gpu_interval_t* interval,
  uint64_t start,
  uint64_t end
)
{
  interval->start = start;
  interval->end = end;
}
