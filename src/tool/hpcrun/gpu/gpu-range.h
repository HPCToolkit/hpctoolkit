#ifndef gpu_range_h
#define gpu_range_h

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


uint64_t
gpu_range_correlation_id
(
);


uint64_t
gpu_range_enter
(
);


void
gpu_range_exit
(
);


void
gpu_range_lead_barrier
(
);


bool
gpu_range_is_lead
(
);

#endif  // gpu_range_h
