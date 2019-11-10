#ifndef _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_
#define _HPCTOOLKIT_CUDA_STATE_PLACEHOLDERS_H_

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>




typedef struct {
<<<<<<< HEAD
  placeholder_t cuda_none_state;
  placeholder_t cuda_memcpy_state;
  placeholder_t cuda_memalloc_state;
  placeholder_t cuda_kernel_state;
  placeholder_t cuda_sync_state;
=======
  cuda_placeholder_t cuda_none_state;
  cuda_placeholder_t cuda_copy_state;
  cuda_placeholder_t cuda_alloc_state;
  cuda_placeholder_t cuda_delete_state;
  cuda_placeholder_t cuda_kernel_state;
  cuda_placeholder_t cuda_sync_state;
>>>>>>> master-gpu-trace
} cuda_placeholders_t;

extern cuda_placeholders_t cuda_placeholders;

void
cuda_init_placeholders
(
);

#endif
