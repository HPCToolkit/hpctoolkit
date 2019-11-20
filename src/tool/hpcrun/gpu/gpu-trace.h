#ifndef gpu_trace_h
#define gpu_trace_h

//******************************************************************************
// global includes
//******************************************************************************

#include <inttypes.h>



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;

typedef struct thread_data_t thread_data_t;

typedef struct gpu_trace_channel_t gpu_trace_channel_t;

typedef struct gpu_trace_t gpu_trace_t;

typedef struct gpu_trace_item_t gpu_trace_item_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*gpu_trace_fn_t)
(
 gpu_trace_t *trace, 
 gpu_trace_item_t *ti
);



//******************************************************************************
// interface operations
//******************************************************************************

void 
gpu_trace_init
(
 void
);


gpu_trace_t *
gpu_trace_create
(
 void
);


void *
gpu_trace_record
(
 gpu_trace_t *thread_args
);


void 
gpu_trace_produce
(
 gpu_trace_t *t,
 gpu_trace_item_t *ti
);


void 
gpu_trace_signal_consumer
(
 gpu_trace_t *t
);


void
gpu_trace_fini
(
 void *arg
);



#endif 
