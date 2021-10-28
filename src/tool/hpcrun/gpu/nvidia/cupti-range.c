#include "cupti-range.h"

#include <string.h>
#include <stdbool.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-range.h>
#include <hpcrun/gpu/gpu-correlation-id.h>

#include "cuda-api.h"
#include "cupti-api.h"
#include "cupti-ip-norm-map.h"
#include "cupti-pc-sampling-api.h"
#include "cupti-cct-trace.h"
#include "cupti-range-thread-list.h"


static cupti_range_mode_t cupti_range_mode = CUPTI_RANGE_MODE_NONE;

static uint32_t cupti_range_interval = CUPTI_RANGE_DEFAULT_INTERVAL;
static uint32_t cupti_range_sampling_period = CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD;
static uint32_t cupti_range_thread_retain_range = CUPTI_RANGE_THREAD_RETAIN_RANGE;

static uint32_t cupti_retained_ranges = 0;

static __thread cct_node_t *prev_api_node = NULL;

static bool
cupti_range_pre_enter_callback
(
 uint64_t correlation_id,
 void *args
)
{
  TMSG(CUPTI_TRACE, "Enter CUPTI range pre correlation_id %lu", correlation_id);
  return cupti_range_mode != CUPTI_RANGE_MODE_NONE;
}


static void
cupti_range_kernel_count_increase
(
 cct_node_t *kernel_ph,
 uint32_t context_id,
 uint32_t range_id,
 uint64_t sampled_count,
 uint64_t count
)
{
  // Range processing
  cct_node_t *node = hpcrun_cct_children(kernel_ph);
  node = hpcrun_cct_insert_context(node, context_id);
  node = hpcrun_cct_insert_range(node, range_id);

  // Increase kernel count
  gpu_metrics_attribute_kernel_count(node, sampled_count, count);
}


static bool
cupti_range_mode_even_is_enter
(
 uint64_t correlation_id
)
{
  return (GPU_CORRELATION_ID_UNMASK(correlation_id) % cupti_range_interval) == 0;
}


static bool
cupti_range_mode_context_sensitive_is_sampled
(
)
{
  int left = rand() % cupti_range_sampling_period;
  if (left == 0) {
    return true;
  } else {
    return false;
  }
}


static bool
cupti_range_mode_context_sensitive_is_enter
(
 cct_node_t *kernel_ph,
 uint64_t correlation_id,
 uint32_t range_id
)
{
  thread_data_t *cur_td = hpcrun_safe_get_td();
  core_profile_trace_data_t *cptd = &cur_td->core_profile_trace_data;
  cupti_range_thread_list_add(cptd->id);
  bool is_cur = cupti_range_thread_list_is_cur(cptd->id);

  cct_node_t *kernel_node = hpcrun_cct_children(kernel_ph);
  ip_normalized_t kernel_ip = hpcrun_cct_addr(kernel_node)->ip_norm;
  cct_node_t *api_node = hpcrun_cct_parent(kernel_ph);

  CUcontext context;
  cuda_context_get(&context);
  cupti_ip_norm_map_ret_t map_ret_type = cupti_ip_norm_map_lookup_thread(kernel_ip, api_node);
  bool active = cupti_pc_sampling_active();

  if (map_ret_type == CUPTI_IP_NORM_MAP_DUPLICATE) {
    // There must be a logical flush
    int32_t prev_range_id = cupti_cct_trace_flush(range_id, active, is_cur);
    if (is_cur) {
      if (active) {
        // If active, we encounter a new range and have to flush.
        // It is an early collection mode different than other modes
        if (prev_range_id != -1) {
          cupti_pc_sampling_range_context_collect(prev_range_id, context);
        } else {
          cupti_pc_sampling_range_context_collect(range_id, context);
        }
      }
      cupti_retained_ranges++;
      if (cupti_retained_ranges == cupti_range_thread_retain_range) {
        cupti_retained_ranges = 0;
        cupti_range_thread_list_advance_cur(); 
        is_cur = cupti_range_thread_list_is_cur(cptd->id);
      }
    }
    // After flushing, we clean up ccts in the previous range
    cupti_ip_norm_map_clear_thread();
    cupti_ip_norm_map_insert_thread(kernel_ip, api_node);
    // Update active status
    active = cupti_pc_sampling_active();
  } else if (map_ret_type == CUPTI_IP_NORM_MAP_NOT_EXIST) {
    // No such a node
    cupti_ip_norm_map_insert_thread(kernel_ip, api_node);
  }

  bool repeated = cupti_cct_trace_append(range_id, api_node);
  if (is_cur && !active) {
    // 1. abc | (a1)bc
    // 2. abc | ... | abc
    bool sampled = !repeated ||
      (map_ret_type == CUPTI_IP_NORM_MAP_DUPLICATE && cupti_range_mode_context_sensitive_is_sampled());
    if (sampled) {
      TMSG(CUPTI_TRACE, "Range repeated %d, map_ret_type %d, api_node %p", repeated, map_ret_type, api_node);
      cupti_pc_sampling_start(context);
    }
  }

  return map_ret_type == CUPTI_IP_NORM_MAP_DUPLICATE;
}


static bool
cupti_range_post_enter_callback
(
 uint64_t correlation_id,
 void *args
)
{
  TMSG(CUPTI_TRACE, "Enter CUPTI range post correlation_id %lu", correlation_id);

  CUcontext context;
  cuda_context_get(&context);
	uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  uint32_t range_id = gpu_range_id_get();
  cct_node_t *kernel_ph = (cct_node_t *)args;

  bool ret = false;

  if (cupti_range_mode == CUPTI_RANGE_MODE_EVEN) {
    ret = cupti_range_mode_even_is_enter(correlation_id);

    // Increase kernel count for postmortem apportion based on counts
    cupti_range_kernel_count_increase(kernel_ph, context_id, range_id, 1, 1);
  } else if (cupti_range_mode == CUPTI_RANGE_MODE_CONTEXT_SENSITIVE) {
    ret = cupti_range_mode_context_sensitive_is_enter(kernel_ph, correlation_id, range_id);
    // Just create a placeholder range node
    uint64_t sampled_count = cupti_pc_sampling_active() ? 1 : 0;
    cupti_range_kernel_count_increase(kernel_ph, context_id, range_id, sampled_count, 1);
  }

  // TODO(Keren): check if pc sampling buffer is full
  // If full, ret = true

  return ret;
}


static bool
cupti_range_pre_exit_callback
(
 uint64_t correlation_id,
 void *args
)
{
  TMSG(CUPTI_TRACE, "Exit CUPTI range pre correlation_id %lu", correlation_id);

  return cupti_range_mode != CUPTI_RANGE_MODE_NONE;
}


static bool
cupti_range_post_exit_callback
(
 uint64_t correlation_id,
 void *args
)
{
  TMSG(CUPTI_TRACE, "Exit CUPTI range post correlation_id %lu", correlation_id);

  CUcontext context;
  cuda_context_get(&context);

  if (cupti_range_mode == CUPTI_RANGE_MODE_SERIAL) {
    // Collect pc samples from the current context
    cupti_pc_sampling_correlation_context_collect(cupti_kernel_ph_get(), context);
  } else if (cupti_range_mode == CUPTI_RANGE_MODE_EVEN) {
    if (gpu_range_is_lead()) {
      // Collect pc samples from all contexts
      uint32_t range_id = gpu_range_id_get();

      cupti_pc_sampling_range_context_collect(range_id, context);
      // Restart pc sampling immediately
      cupti_pc_sampling_start(context);
    }
  }

  return false;
}


void
cupti_range_config
(
 const char *mode_str,
 int interval,
 int sampling_period
)
{
  TMSG(CUPTI, "Enter cupti_range_config mode %s, interval %d, sampling period %d", mode_str, interval, sampling_period);

  gpu_range_enable();

  cupti_range_interval = interval;
  cupti_range_sampling_period = sampling_period;

  // Range mode is only enabled with option "gpu=nvidia,pc"
  //
  // In the even mode, pc samples are collected for every n kernels.
  // If n == 1, we fall back to the serialized pc sampling collection,
  // in which pc samples are collected after every kernel launch.
  //
  // In the context sensitive mode, pc samples are flushed based on
  // the number of kernels belong to different contexts.
  // We don't flush pc samples unless a kernel in the range is launched
  // by two different contexts.
  if (strcmp(mode_str, "EVEN") == 0) {
    if (interval > CUPTI_RANGE_DEFAULT_INTERVAL) {
      cupti_range_mode = CUPTI_RANGE_MODE_EVEN;
    } else {
      cupti_range_mode = CUPTI_RANGE_MODE_SERIAL;
    }
  } else if (strcmp(mode_str, "CONTEXT_SENSITIVE") == 0) {
    cupti_range_mode = CUPTI_RANGE_MODE_CONTEXT_SENSITIVE;
  }

  if (cupti_range_mode != CUPTI_RANGE_MODE_NONE) {
    gpu_range_enter_callbacks_register(cupti_range_pre_enter_callback,
      cupti_range_post_enter_callback);
    gpu_range_exit_callbacks_register(cupti_range_pre_exit_callback,
      cupti_range_post_exit_callback);
  }

  TMSG(CUPTI, "Enter cupti_range_config");
}


cupti_range_mode_t
cupti_range_mode_get
(
 void
)
{
  return cupti_range_mode;
}


uint32_t
cupti_range_interval_get
(
 void
)
{
  return cupti_range_interval;
}


uint32_t
cupti_range_sampling_period_get
(
 void
)
{
  return cupti_range_sampling_period;
}
