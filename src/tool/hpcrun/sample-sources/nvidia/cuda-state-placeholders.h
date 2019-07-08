#ifndef _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_
#define _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_

#include <hpcrun/utilities/ip-normalized.h>

typedef struct {
  void           *pc;
  ip_normalized_t pc_norm; 
} cuda_placeholder_t;


typedef struct {
  cuda_placeholder_t cuda_none_state;
  cuda_placeholder_t cuda_memcpy_state;
  cuda_placeholder_t cuda_memalloc_state;
  cuda_placeholder_t cuda_kernel_state;
  cuda_placeholder_t cuda_sync_state;
} cuda_placeholders_t;

extern cuda_placeholders_t cuda_placeholders;

void
cuda_init_placeholders
(
);

#endif
