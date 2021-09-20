#include "cupti-range.h"

#include <string.h>
#include <stdbool.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-range.h>
#include <hpcrun/gpu/gpu-correlation-id.h>

#include "cuda-api.h"
#include "cupti-api.h"
#include "cupti-pc-sampling-api.h"


static cupti_range_mode_t cupti_range_mode = CUPTI_RANGE_MODE_NONE;

static uint32_t cupti_range_interval = CUPTI_RANGE_DEFAULT_INTERVAL;

static bool
cupti_range_pre_enter_callback
(
 uint64_t correlation_id,
 void *args
)
{
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
cupti_range_post_enter_callback
(
 uint64_t correlation_id,
 void *args
)
{
  CUcontext context;
  cuda_context_get(&context);
	uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  uint32_t range_id = gpu_range_id_get();

  bool ret = false;

  if (cupti_range_mode == CUPTI_RANGE_MODE_EVEN) {
    ret = cupti_range_mode_even_is_enter(correlation_id);
  } else if (cupti_range_mode == CUPTI_RANGE_MODE_CONTEXT_SENSITIVE) {
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
  return cupti_kernel_ph_get() != NULL;
}


static bool
cupti_range_post_exit_callback
(
 uint64_t correlation_id,
 void *args
)
{
  if (cupti_range_mode == CUPTI_RANGE_MODE_NONE) {
    // Collect pc samples from the current context
    CUcontext context;
    cuda_context_get(&context);
    cupti_pc_sampling_correlation_context_collect(cupti_kernel_ph_get(), context);
  } else if (cupti_range_mode == CUPTI_RANGE_MODE_EVEN || CUPTI_RANGE_MODE_CONTEXT_SENSITIVE) {
    if (gpu_range_is_lead()) {
      // Collect pc samples from all contexts
      uint32_t range_id = gpu_range_id_get();
      cupti_pc_sampling_range_collect(range_id);
    }
  }

  return false;
}


void
cupti_range_config
(
 const char *mode_str,
 int interval
)
{
  gpu_range_enable();

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
  if (strcmp(mode_str, "EVEN") == 0 && interval > CUPTI_RANGE_DEFAULT_INTERVAL) {
    cupti_range_interval = interval;
    cupti_range_mode = CUPTI_RANGE_MODE_EVEN;
  } else if (strcmp(mode_str, "CONTEXT_SENSITIVE") == 0) {
    cupti_range_mode = CUPTI_RANGE_MODE_CONTEXT_SENSITIVE;
  }

  if (cupti_range_mode != CUPTI_RANGE_MODE_NONE) {
    gpu_range_enter_callbacks_register(cupti_range_pre_enter_callback,
      cupti_range_post_enter_callback);
    gpu_range_exit_callbacks_register(cupti_range_pre_exit_callback,
      cupti_range_post_exit_callback);
  }
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
