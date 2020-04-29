#ifndef sample_sources_nvidia_h
#define sample_sources_nvidia_h

//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cuda.h>



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;
typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

void cupti_activity_attribute(gpu_activity_t *activity);

int cupti_pc_sampling_frequency_get();

int cupti_trace_frequency_get();


#endif
