#include "gpu-range.h"

#include <unistd.h>

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/cct/cct_bundle.h>
#include <hpcrun/rank.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/write_data.h>

#include "gpu-metrics.h"

static atomic_ullong lead_correlation_id = ATOMIC_VAR_INIT(0);
static atomic_ullong correlation_id_done = ATOMIC_VAR_INIT(0);
static atomic_uint range_id = ATOMIC_VAR_INIT(GPU_RANGE_DEFAULT_RANGE);

static __thread uint64_t thread_correlation_id = 0;
static __thread uint32_t thread_range_id = GPU_RANGE_DEFAULT_RANGE;

static spinlock_t count_lock = SPINLOCK_UNLOCKED;

static bool gpu_range_enable_status = false;

static bool
gpu_range_callback_default
(
 uint64_t correlation_id,
 void *args
)
{
  return false;
}

static gpu_range_callback_t gpu_range_pre_enter_callback = gpu_range_callback_default;

static gpu_range_callback_t gpu_range_post_enter_callback = gpu_range_callback_default;

static gpu_range_callback_t gpu_range_pre_exit_callback = gpu_range_callback_default;

static gpu_range_callback_t gpu_range_post_exit_callback = gpu_range_callback_default;

void
gpu_range_profile_dump
(
)
{
  thread_data_t *cur_td = hpcrun_safe_get_td();

  core_profile_trace_data_t *cptd = &cur_td->core_profile_trace_data;

  hpcrun_id_tuple_cputhread(cur_td);

  hpcrun_safe_enter();

  hpcrun_write_profile_data(cptd);

  hpcrun_safe_exit();
}


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
  cct_node_t *cct_root = NULL;
  if (cct_bundle->gpu_range_root == NULL) {
    cct_bundle->gpu_range_root = hpcrun_cct_top_new(HPCRUN_FMT_GPU_RANGE_ROOT_NODE, 0);
  }
  cct_root = cct_bundle->gpu_range_root;
  cct_node_t *cct_context = hpcrun_cct_insert_context(cct_root, context_id);
  cct_node_t *cct_range = hpcrun_cct_insert_range(cct_context, range_id);

  hpcrun_safe_exit();

  return cct_range;
}


uint32_t
gpu_range_id_get
(
 void
)
{
  return thread_range_id;
}


uint64_t
gpu_range_enter
(
 cct_node_t *api_node,
 uint64_t correlation_id
)
{
  if (!gpu_range_enabled()) {
    return GPU_RANGE_DEFAULT_RANGE;
  } 

  thread_correlation_id = correlation_id;

  if (!gpu_range_pre_enter_callback(thread_correlation_id, api_node)) {
    return GPU_RANGE_DEFAULT_RANGE;
  }

  spinlock_lock(&count_lock);

  // Early update of range_id
  thread_range_id = atomic_load(&range_id);

  bool is_lead = gpu_range_post_enter_callback(thread_correlation_id, api_node);
  if (is_lead) {
    // The last call in this range
    atomic_store(&lead_correlation_id, thread_correlation_id);

    atomic_fetch_add(&range_id, 1);

    // In case myself update the range_id
    thread_range_id = atomic_load(&range_id);
  }

  return thread_range_id;
}


bool
gpu_range_is_lead
(
 void
)
{
  return thread_correlation_id == atomic_load(&lead_correlation_id);
}

void
gpu_range_exit
(
 void
)
{
  if (!gpu_range_enabled()) {
    return;
  } 

  if (!gpu_range_pre_exit_callback(thread_correlation_id, NULL)) {
    return;
  }

  // Wait until correlation id before me have done
  gpu_range_post_exit_callback(thread_correlation_id, NULL);

  // Unleash wait operations in range enter
  if (gpu_range_is_lead()) {
    atomic_store(&lead_correlation_id, 0);
  } 

  spinlock_unlock(&count_lock);
}

bool
gpu_range_enabled
(
 void
)
{
  return gpu_range_enable_status;
}

// Enables range profiling
void
gpu_range_enable
(
 void
)
{
  gpu_range_enable_status = true;
}


void
gpu_range_disable
(
 void
)
{
  gpu_range_enable_status = false;
}


void
gpu_range_enter_callbacks_register
(
 gpu_range_callback_t pre_callback,
 gpu_range_callback_t post_callback
)
{
  gpu_range_pre_enter_callback = pre_callback;
  gpu_range_post_enter_callback = post_callback;
}


void
gpu_range_exit_callbacks_register
(
 gpu_range_callback_t pre_callback,
 gpu_range_callback_t post_callback
)
{
  gpu_range_pre_exit_callback = pre_callback;
  gpu_range_post_exit_callback = post_callback;
}
