#include "gpu-range.h"

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct_bundle.h>
#include <hpcrun/threadmgr.h>

static atomic_uint thread_id = ATOMIC_VAR_INIT(1);
static atomic_uint thread_id_lead = ATOMIC_VAR_INIT(0);
static atomic_bool wait = ATOMIC_VAR_INIT(false);

static __thread uint32_t thread_range_id = 0;
static __thread uint32_t thread_id_local = 0;

static spinlock_t count_lock = SPINLOCK_UNLOCKED;

static uint32_t
gpu_range_thread_id
(
 uint32_t range_id
)
{
  // FIXME: this is a bad way to compute a stream id
  uint32_t id = THREAD_ID_GPU_RANGE + range_id;

  return id;
}


thread_data_t *
gpu_range_thread_data_acquire
(
 uint32_t range_id
)
{
  bool demand_new_thread = false;
  bool has_trace = false;

  thread_data_t *td = NULL;

  uint32_t thread_id = gpu_range_thread_id(range_id);

  hpcrun_threadMgr_data_get_safe(td, NULL, &td, has_trace, demand_new_thread);

  return td;
}


void
gpu_range_attribute
(
 uint32_t context_id,
 gpu_activity_t *activity
)
{
  // Set current thread data
  thread_data_t *cur_td = hpcrun_safe_get_td();
  thread_data_t *range_td = gpu_range_thread_data_acquire(activity->range_id);
  hpcrun_set_thread_data(range_td);

  cct_bundle_t *cct_bundle = &(range_td->core_profile_trace_data.epoch->csdata);
  cct_node_t *cct_root = cct_bundle->top;
  cct_node_t *cct_context = hpcrun_cct_insert_context(cct_root, context_id);

  // Set current activity data
  gpu_activity_t *cct_node = activity->cct_node;
  activity->cct_node = cct_context;

  gpu_metrics_attribute(activity);

  // Reset thread data and activity data
  hpcrun_set_thread_data(cur_td);
  activity->cct_node = cct_node;
}


uint32_t
gpu_range_id
(
)
{
  return thread_range_id;
}


void
gpu_range_enter
(
 uint64_t correlation_id
)
{
  thread_range_id = (GPU_CORRELATION_ID_UNMASK(correlation_id) - 1) / GPU_RANGE_COUNT_LIMIT;

  if (thread_id_local == 0) {
    thread_id_local = atomic_fetch_add(&thread_id, 1);
  }

  spinlock_lock(&count_lock);

  if (GPU_CORRELATION_ID_UNMASK(correlation_id) % GPU_RANGE_COUNT_LIMIT == 0) {
    // The last call in this range
    atomic_store(&thread_id_lead, thread_id_local);
    atomic_store(&wait, true);
  }

  spinlock_unlock(&count_lock);

  while (atomic_load(&thread_id_lead) != thread_id_local && atomic_load(&wait) == true) {}
}


void
gpu_range_exit
(
)
{
  if (gpu_range_is_lead()) {
    atomic_store(&thread_id_lead, 0);
    atomic_store(&wait, false);
  }
}


bool
gpu_range_is_lead
(
)
{
  return atomic_load(&thread_id_lead) == thread_id_local;
}
