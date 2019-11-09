#ifndef gpu_correlation_channel_h
#define gpu_correlation_channel_h


//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>

#include "gpu-correlation.h"
#include "gpu-activity-channel.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t gpu_correlation_channel_t;



//******************************************************************************
// interface operations 
//******************************************************************************

// produce into a channel that my thread created
void
gpu_correlation_channel_produce
(
 uint64_t host_op_id,
 cct_node_t *api_node,
 cct_node_t *func_node
);


// consume from a channel that another thread created
void
gpu_correlation_channel_consume
(
 gpu_correlation_channel_t *channel
);



#endif
