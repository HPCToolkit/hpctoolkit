#ifndef _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_
#define _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/sample-sources/gpu/placeholders.h>




typedef struct {
  placeholder_t cuda_none_state;
  placeholder_t cuda_memcpy_state;
  placeholder_t cuda_memalloc_state;
  placeholder_t cuda_kernel_state;
  placeholder_t cuda_sync_state;
} cuda_placeholders_t;

extern cuda_placeholders_t cuda_placeholders;

void
cuda_init_placeholders
(
);

#endif
