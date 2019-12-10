#ifndef gpu_activity_channel_h
#define gpu_activity_channel_h

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>

#include "gpu-activity.h"


//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t; 

typedef struct gpu_activity_channel_t gpu_activity_channel_t; 



//******************************************************************************
// interface operations
//******************************************************************************

gpu_activity_channel_t *
gpu_activity_channel_get
(
 void
);


void
gpu_activity_channel_produce
(
 gpu_activity_channel_t *channel,
 gpu_activity_t *a
);


void
gpu_activity_channel_consume
(
 gpu_activity_attribute_fn_t aa_fn
);



#endif
