#ifndef gpu_range_h
#define gpu_range_h

#include <stdint.h>
#include <stdbool.h>

#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>

#include "gpu-correlation-id.h"
#include "gpu-activity.h"
#include "gpu-op-placeholders.h"

#define GPU_RANGE_DEFAULT_RANGE 0

// Range enter/exit callbacks
typedef bool (*gpu_range_callback_t)
(
 uint64_t correlation_id,
 void *args
);

// Writes a range profile to disk
void
gpu_range_profile_dump
(
 void
);

// Gets a range node under the specific context with range_id
cct_node_t *
gpu_range_context_cct_get
(
 uint32_t range_id,
 uint32_t context_id
);

// Gets the current range_id, default 0
uint32_t
gpu_range_id_get
(
 void
);

// Gets the leading correlation id in this range
uint64_t
gpu_range_correlation_id_get
(
 void
);

// Enters a GPU range and invokes gpu_range_enter_callback.
// And returns the correlation id assigned to the CPU thread
uint64_t
gpu_range_enter
(
 cct_node_t *api_node
);

// Exits a GPU range and invokes gpu_range_exit_callback
void
gpu_range_exit
(
 void
);

// Returns true if range profiling is enabled
bool
gpu_range_enabled
(
 void
);

// Enables range profiling
void
gpu_range_enable
(
 void
);

// Returns true if the CPU thread is the lead
bool
gpu_range_is_lead
(
 void
);

// Registers callback functions when entering a gpu range
void
gpu_range_enter_callbacks_register
(
 gpu_range_callback_t pre_callback,
 gpu_range_callback_t post_callback
);

// Registers callback functions when exiting a gpu range
void
gpu_range_exit_callbacks_register
(
 gpu_range_callback_t pre_callback,
 gpu_range_callback_t post_callback
);

#endif  // gpu_range_h
