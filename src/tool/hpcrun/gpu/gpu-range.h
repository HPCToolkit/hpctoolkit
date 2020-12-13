#ifndef gpu_trace_h
#define gpu_trace_h

#include <stdint.h>
#include <stdbool.h>

#include <hpcrun/thread_data.h>

#include "gpu-correlation-id.h"
#include "gpu-activity.h"


#define GPU_RANGE_COUNT_LIMIT 2000

thread_data_t *
gpu_range_thread_data_acquire
(
 uint32_t range_id
);


void
gpu_range_attribute
(
 uint32_t context_id,
 gpu_activity_t *activity 
);


uint32_t
gpu_range_id
(
);


void
gpu_range_enter
(
 uint64_t correlation_id
);


void
gpu_range_exit
(
);


bool
gpu_range_is_lead
(
);

#endif  // gpu_trace_h
