//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-channel-item-allocator.h"



//******************************************************************************
// interface functions
//******************************************************************************

s_element_t *
channel_item_alloc_helper
(
 bichannel_t *c, 
 size_t size
)
{
  s_element_t *se = bichannel_pop(c, bichannel_direction_backward);
  if (!se) {
    bichannel_steal(c, bichannel_direction_backward);
    se = bichannel_pop(c, bichannel_direction_backward);
  }
  if (!se) {
    se = (s_element_t *) hpcrun_malloc_safe(size);
    sstack_ptr_set(&se->next, 0);
  }
  return se;
}


void
channel_item_free_helper
(
 bichannel_t *c, 
 s_element_t *se
)
{
  bichannel_push(c, bichannel_direction_backward, se);
}
