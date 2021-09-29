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


static cupti_range_mode_t cupti_range_mode = CUPTI_RANGE_MODE_NONE;

static uint32_t cupti_range_interval = CUPTI_RANGE_DEFAULT_INTERVAL;
static uint32_t cupti_range_sampling_period = CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD;

static bool
cupti_range_pre_enter_callback
(
 uint64_t correlation_id,
 void *args
)
{
  TMSG(CUPTI_TRACE, "Enter CUPTI range pre correlation_id %lu", correlation_id);
  return cupti_kernel_ph_get() != NULL && cupti_range_mode != CUPTI_RANGE_MODE_NONE;
}


static void
increase_kernel_count
(
 cct_node_t *kernel_ph,
 uint32_t context_id,
 uint32_t range_id
)
{
  // Range processing
  cct_node_t *kernel_node = hpcrun_cct_children(kernel_ph);
  kernel_node = hpcrun_cct_insert_context(kernel_node, context_id);
  kernel_node = hpcrun_cct_insert_range(kernel_node, range_id);

  // Increase kernel count
  gpu_metrics_attribute_kernel_count(kernel_node);
}


static bool
cupti_range_mode_even_is_enter
(
 uint64_t correlation_id
)
{
  return GPU_CORRELATION_ID_UNMASK(correlation_id) % cupti_range_interval;
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
 uint64_t correlation_id,
 void *args,
 uint32_t range_id
)
{
  cct_node_t *api_node = args;
  cupti_ip_norm_map_ret_t map_ret_type = cupti_ip_norm_map_lookup(api_node);

  bool do_flush = false;
  if (map_ret_type == CUPTI_IP_NORM_MAP_DUPLICATE) {
    // Duplicate ccts
    cupti_ip_norm_map_clear();
    if (cupti_pc_sampling_active()) {
      // If active, we encounter a new range and have to flush
      do_flush = true;
    }
  } else if (map_ret_type == CUPTI_IP_NORM_MAP_NOT_EXIST) {
    // No such a node
    cupti_ip_norm_map_insert(api_node);
  }

  CUcontext context;
  cuda_context_get(&context);

  if (do_flush) {
    // Early collection, different than other modes
    cupti_pc_sampling_range_context_collect(range_id, context);
    cupti_cct_trace_flush();
  }

  if (!cupti_pc_sampling_active()) {
    bool sampled = cupti_cct_trace_append(api_node) && cupti_range_mode_context_sensitive_is_sampled();
    if (sampled) {
      cupti_pc_sampling_start(context);
    }
  }

  return do_flush;
#if 0
  bool exist_intrie = cupti_cct_trie_lookup(api_node);
  bool do_flush = false;
  bool unwind = false;
  if (map_ret_type == CUPTI_IP_NORM_MAP_DUPLICATE) {
    // Duplicate ccts
    cupti_cct_set_clear();
    if (cupti_pc_sampling_active()) {
      // If active, we encounter a new range and have to flush
      do_flush = true;
    }

    // We unwind CCT to the root
    cupti_cct_trie_unwind();
    exist_intrie = cupti_cct_trie_lookup(api_node);
    unwind = true;
  } else if (map_ret_type == CUPTI_IP_NORM_MAP_NOT_EXIST) {
    // No such a node
    cupti_cct_set_insert(api_node);
  } else {
    // Same cct, this is a repeated string
    // We unwind CCT to the root
    cupti_cct_trie_unwind();
    exist_intrie = cupti_cct_trie_lookup(api_node);
    unwind = true;
  }
  cupti_cct_trie_insert(api_node);

  CUcontext context;
  cuda_context_get(&context);

  if (do_flush) {
    // Early collection, different than other modes
    cupti_pc_sampling_range_context_collect(range_id, context);
  }

  bool sampled = false;
  if (unwind) {
    // I was unwind because of loop or flushing
    if (exist_intrie) {
      // The next node is already in the trie.
      // This means we have seen this range before, move forward and sample only if necessary
      if (cupti_range_mode_context_sensitive_is_sampled()) {
        // I was flushed because 
        sampled = true;
      }
    } else {
      // I haven't seen this node bofore, must sample this range
      sampled = true;
    }
  }
  if (sampled) {
    // No need to turn on pc sampling unless sampled
    cupti_pc_sampling_start(context);
  }
#endif
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

  bool ret = false;

  if (cupti_range_mode == CUPTI_RANGE_MODE_EVEN) {
    ret = cupti_range_mode_even_is_enter(correlation_id);
  } else if (cupti_range_mode == CUPTI_RANGE_MODE_CONTEXT_SENSITIVE) {
    ret = cupti_range_mode_context_sensitive_is_enter(correlation_id, args, range_id);
  }

  // TODO(Keren): check if pc sampling buffer is full
  // If full, ret = true

  // Increase kernel count for postmortem apportion based on counts
  increase_kernel_count(cupti_kernel_ph_get(), context_id, range_id);

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

  return cupti_kernel_ph_get() != NULL;
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
      // Start pc sampling immediately
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
