#include "gpu-range.h"

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct_bundle.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/safe-sampling.h>

#include "gpu-metrics.h"

static atomic_ullong correlation_id_lead = ATOMIC_VAR_INIT(0);
static atomic_ullong correlation_id_done = ATOMIC_VAR_INIT(0);

static __thread uint64_t thread_correlation_id = 0;
static __thread uint32_t thread_range_id = 0;

static uint32_t gpu_range_interval = 1;

static spinlock_t count_lock = SPINLOCK_UNLOCKED;

cct_node_t *
gpu_range_context_cct_get
(
 uint32_t range_id,
 uint32_t context_id
)
{
  // Set current thread data
  thread_data_t *cur_td = hpcrun_safe_get_td();

  hpcrun_safe_enter();

  cct_bundle_t *cct_bundle = &(cur_td->core_profile_trace_data.epoch->csdata);
  cct_node_t *cct_root = cct_bundle->top;
  cct_node_t *cct_range = hpcrun_cct_insert_range(cct_root, range_id);
  cct_node_t *cct_context = hpcrun_cct_insert_context(cct_range, context_id);

  hpcrun_safe_exit();

  return cct_context;
}


uint32_t
gpu_range_id
(
)
{
  return thread_range_id;
}


uint64_t
gpu_range_correlation_id
(
)
{
  return thread_correlation_id;
}


uint64_t
gpu_range_enter
(
)
{
  spinlock_lock(&count_lock);

  uint64_t correlation_id = gpu_correlation_id();
  if (GPU_CORRELATION_ID_UNMASK(correlation_id) % gpu_range_interval == 0) {
    // If lead is set, don't do anything
    while (atomic_load(&correlation_id_lead) != 0) {}
    // The last call in this range
    atomic_store(&correlation_id_lead, correlation_id);
  }

  spinlock_unlock(&count_lock);

  thread_correlation_id = correlation_id;
  thread_range_id = (GPU_CORRELATION_ID_UNMASK(correlation_id) - 1) / gpu_range_interval;

  uint64_t old_correlation_id_lead = atomic_load(&correlation_id_lead);
  // Wait until correlation_id_lead is done
  // If correlation_id_lead is not set, we are good to go
  while (old_correlation_id_lead != 0 && old_correlation_id_lead < correlation_id) {
    old_correlation_id_lead = atomic_load(&correlation_id_lead);
  }

  return correlation_id;
}


void
gpu_range_exit
(
)
{
  atomic_fetch_add(&correlation_id_done, 1);

  if (gpu_range_is_lead()) {
    // unleash wait operations
    atomic_store(&correlation_id_lead, 0);
  }
}


void
gpu_range_lead_barrier
(
)
{
  // Wait until correlation id before me have done
  while (atomic_load(&correlation_id_done) != GPU_CORRELATION_ID_UNMASK(thread_correlation_id) - 1);
}


bool
gpu_range_is_lead
(
)
{
  return atomic_load(&correlation_id_lead) == thread_correlation_id;
}


uint32_t
gpu_range_interval_get
(
)
{
  return gpu_range_interval;
}


void
gpu_range_interval_set
(
 uint32_t interval
)
{
  gpu_range_interval = interval;
}
