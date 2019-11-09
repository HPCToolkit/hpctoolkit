#ifndef gpu_correlation_h
#define gpu_correlation_h

//******************************************************************************
// global includes
//******************************************************************************

#include <inttypes.h>



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST_CORRELATION_HEADER 0



//******************************************************************************
// local includes
//******************************************************************************

// #include <lib/prof-lean/stacks.h>


#include "gpu-activity-channel.h"
#include "gpu-correlation-channel.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;

typedef struct gpu_correlation_channel_t gpu_correlation_channel_t;

typedef struct gpu_correlation_t gpu_correlation_t;

#if 0
typedef struct gpu_correlation_t {
  s_element_t next;

  // correlation info
  uint64_t host_op_id;
  cct_node_t *api_node;
  cct_node_t *func_node;

  // where to report the activity
  gpu_activity_channel_t *activity_channel;
} gpu_correlation_t;
#endif



//******************************************************************************
// interface operations 
//******************************************************************************

gpu_correlation_t *
gpu_correlation_alloc
(
 gpu_correlation_channel_t *channel
);


void
gpu_correlation_free
(
 gpu_correlation_channel_t *channel, 
 gpu_correlation_t *c
);


void
gpu_correlation_produce
(
 gpu_correlation_t *c,
 uint64_t host_op_id,
 cct_node_t *api_node,
 cct_node_t *func_node,
 gpu_activity_channel_t *activity_channel
);


void
gpu_correlation_consume
(
 gpu_correlation_t *c
);



#endif
