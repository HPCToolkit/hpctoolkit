#ifndef gpu_application_thread_api_h
#define gpu_application_thread_api_h



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-activity.h"



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_application_thread_process_activities
(
 gpu_activity_attribute_fn_t aa_fn
);


cct_node_t *
gpu_application_thread_correlation_callback
(
 uint64_t correlation_id
);



#endif
