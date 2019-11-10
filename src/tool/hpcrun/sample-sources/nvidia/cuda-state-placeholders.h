#ifndef cuda_state_placeholders_h
#define cuda_state_placeholders_h

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>


typedef struct {
  placeholder_t cuda_none_state;
  placeholder_t cuda_copy_state;
  placeholder_t cuda_alloc_state;
  placeholder_t cuda_delete_state;
  placeholder_t cuda_kernel_state;
  placeholder_t cuda_sync_state;
} cuda_placeholders_t;

extern cuda_placeholders_t cuda_placeholders;

void
cuda_init_placeholders
(
);

#endif
