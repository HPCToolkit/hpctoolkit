#ifndef gpu_correlation_channel_set_h
#define gpu_correlation_channel_set_h



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t gpu_correlation_channel_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*gpu_correlation_channel_fn_t)
(
 gpu_correlation_channel_t *channel
);



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_correlation_channel_set_insert
(
 gpu_correlation_channel_t *channel
);


void
gpu_correlation_channel_set_consume
(
 void
);

#endif
