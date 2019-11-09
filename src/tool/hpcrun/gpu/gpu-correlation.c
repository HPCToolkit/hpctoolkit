//******************************************************************************
// global includes
//******************************************************************************

#include <stdio.h>



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-correlation.h"
#include "gpu-correlation-channel.h"
#include "gpu-channel-item-allocator.h"

#if UNIT_TEST == 0
#include "gpu-host-correlation-map.h"
#endif

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_t {
  s_element_t next;

  // correlation info
  uint64_t host_op_id;
  cct_node_t *api_node;
  cct_node_t *func_node;

  // where to report the activity
  gpu_activity_channel_t *activity_channel;
} gpu_correlation_t;



//******************************************************************************
// interface operations 
//******************************************************************************

void
gpu_correlation_produce
(
 gpu_correlation_t *c,
 uint64_t host_op_id,
 cct_node_t *api_node,
 cct_node_t *func_node,
 gpu_activity_channel_t *activity_channel
)
{
  c->host_op_id = host_op_id;
  c->api_node = api_node;
  c->func_node = func_node;
  c->activity_channel = activity_channel;
}


void
gpu_correlation_consume
(
 gpu_correlation_t *c
)
{
#if UNIT_TEST == 0
    PRINT("Insert correlation id %ld\n", c->host_op_id);
    gpu_host_correlation_map_insert(c->host_op_id, c->api_node, c->func_node, 
				    c->activity_channel);
#else
    printf("gpu_correlation_consume(%ld, %ld,%ld)\n", c->host_op_id, 
	   (long) c->api_node, (long) c->func_node);
#endif
}


gpu_correlation_t *
gpu_correlation_alloc
(
 gpu_correlation_channel_t *channel
)
{
  return channel_item_alloc(channel, gpu_correlation_t);
}


void
gpu_correlation_free
(
 gpu_correlation_channel_t *channel, 
 gpu_correlation_t *c
)
{
  channel_item_free(channel, c);
}

