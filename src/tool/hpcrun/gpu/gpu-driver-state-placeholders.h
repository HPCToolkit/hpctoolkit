#ifndef _HPCTOOLKIT_GPU_DRIVER_STATE_PLACEHOLDERS_H_
#define _HPCTOOLKIT_GPU_DRIVER_STATE_PLACEHOLDERS_H_

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/cct/cct.h>

typedef struct {
  void           *pc;
  ip_normalized_t pc_norm; 
} gpu_driver_placeholder_t;


typedef struct {
  gpu_driver_placeholder_t gpu_none_state;
  gpu_driver_placeholder_t gpu_copy_state;  // general copy, d2d, d2a, or a2d
  gpu_driver_placeholder_t gpu_copyin_state;
  gpu_driver_placeholder_t gpu_copyout_state;
  gpu_driver_placeholder_t gpu_alloc_state;
  gpu_driver_placeholder_t gpu_delete_state;
  gpu_driver_placeholder_t gpu_sync_state;
  gpu_driver_placeholder_t gpu_kernel_state;
  gpu_driver_placeholder_t gpu_trace_state;
} gpu_driver_placeholders_t;


typedef struct {
  cct_node_t *copy_node;  // general copy, d2d, d2a, or a2d
  cct_node_t *copyin_node;
  cct_node_t *copyout_node;
  cct_node_t *alloc_node;
  cct_node_t *delete_node;
  cct_node_t *sync_node;
  cct_node_t *kernel_node;
  cct_node_t *trace_node;
} gpu_driver_ccts_t;


extern gpu_driver_placeholders_t gpu_driver_placeholders;

void
gpu_driver_init_placeholders
(
);

#endif
