#ifndef gpu_channel_util_h
#define gpu_channel_util_h



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>
#include <lib/prof-lean/stacks.h>



//******************************************************************************
// macros
//******************************************************************************

#define channel_item_alloc(channel, channel_item_type)		\
  (channel_item_type *) channel_item_alloc_helper		\
  ((bichannel_t *) channel, sizeof(channel_item_type))	

#define channel_item_free(channel, item)			\
    channel_item_free_helper					\
    ((bichannel_t *) channel,					\
     (s_element_t *) item)



//******************************************************************************
// interface functions
//******************************************************************************

s_element_t *
channel_item_alloc_helper
(
 bichannel_t *c, 
 size_t size
);


void
channel_item_free_helper
(
 bichannel_t *c, 
 s_element_t *se
);



#endif
