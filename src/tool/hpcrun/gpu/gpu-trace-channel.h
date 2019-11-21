#ifndef gpu_trace_channel_h
#define gpu_trace_channel_h



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-trace.h"
#include "gpu-trace-item.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_channel_t gpu_trace_channel_t;


//******************************************************************************
// interface operations 
//******************************************************************************

gpu_trace_channel_t *
gpu_trace_channel_alloc
(
 void
);


void
gpu_trace_channel_produce
(
 gpu_trace_channel_t *channel,
 gpu_trace_item_t *ti
);


void
gpu_trace_channel_consume
(
 gpu_trace_channel_t *channel,
 thread_data_t *td, 
 gpu_trace_item_consume_fn_t trace_item_consume
);


void
gpu_trace_channel_await
(
 gpu_trace_channel_t *channel
);


void
gpu_trace_channel_signal_consumer
(
 gpu_trace_channel_t *trace_channel
);



#endif
