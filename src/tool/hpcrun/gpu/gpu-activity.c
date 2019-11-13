//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-activity.h"
#include "gpu-channel-item-allocator.h"



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_activity_consume
(
 gpu_activity_t *activity
)
{
  assert(0);
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
  uint64_t pc
)
{
  insn->pcOffset = pc;
}

void
set_gpu_activity_interval
(
  gpu_activity_interval_t* interval,
  uint64_t start,
  uint64_t end
)
{
  interval->start = start;
  interval->end = end;
}
