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



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_t gpu_trace_t;



//******************************************************************************
// interface operations
//******************************************************************************

void 
gpu_trace_init
(
 void
);


void *
gpu_trace_record
(
 gpu_trace_t *thread_args
);


void
gpu_trace_fini
(
 void
);


#endif 
