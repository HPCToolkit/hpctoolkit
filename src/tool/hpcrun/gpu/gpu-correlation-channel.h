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

typedef struct gpu_op_ccts_t gpu_op_ccts_t;



//******************************************************************************
// interface operations 
//******************************************************************************

// produce into a channel that my thread created
void
gpu_correlation_channel_produce
(
 uint64_t host_correlation_id,
 gpu_op_ccts_t *gpu_ccts
);


// consume from a channel that another thread created
void
gpu_correlation_channel_consume
(
 gpu_correlation_channel_t *channel
);



#endif
