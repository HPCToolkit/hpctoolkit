#ifndef gpu_trace_item_h
#define gpu_trace_item_h

//******************************************************************************
// global includes
//******************************************************************************

#include <inttypes.h>


//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t; 

typedef struct thread_data_t thread_data_t; 



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_channel_t gpu_trace_channel_t; 

typedef struct gpu_trace_item_t {
  cct_node_t *call_path_leaf;
  uint64_t start; 
  uint64_t end; 
} gpu_trace_item_t;


typedef void (*gpu_trace_item_consume_fn_t)
(
 thread_data_t *td,
 cct_node_t *call_path_leaf,
 uint64_t start_time,
 uint64_t end_time
);


typedef void (*gpu_trace_fn_t)
(
 gpu_trace_channel_t *channel, 
 gpu_trace_item_t *ti
);



//******************************************************************************
// interface operations 
//******************************************************************************

void
gpu_trace_item_produce
(
 gpu_trace_item_t *ti,
 uint64_t start,
 uint64_t end,
 cct_node_t *call_path_leaf
);


void
gpu_trace_item_consume
(
 gpu_trace_item_consume_fn_t trace_item_consume,
 thread_data_t *td,
 gpu_trace_item_t *ti
);


gpu_trace_item_t *
gpu_trace_item_alloc
(
 gpu_trace_channel_t *channel
);


void
gpu_trace_item_free
(
 gpu_trace_channel_t *channel, 
 gpu_trace_item_t *ti
);



#endif
