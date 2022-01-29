#ifndef PAPI_METRIC_COLLECTOR_H_
#define PAPI_METRIC_COLLECTOR_H_

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>                                             // cct_node_t
#include <hpcrun/gpu/gpu-activity.h>                                    // gpu_activity_channel_t
#include <hpcrun/gpu/blame-shifting/blame-kernel-map.h>                 // kernel_node_t



//******************************************************************************
// interface operations
//******************************************************************************

void
notify_gpu_util_thr_hpcrun_completion
(
 void
);


void
add_kernel_to_incomplete_list
(
 kernel_node_t *kernel_node
);


void
remove_kernel_from_incomplete_list
(
 kernel_node_t *kernel_node
);


void*
papi_metric_callback
(
 void *arg
);

#endif      // PAPI_METRIC_COLLECTOR_H_