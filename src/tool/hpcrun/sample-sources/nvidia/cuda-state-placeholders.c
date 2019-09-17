#include "cuda-state-placeholders.h"

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

cuda_placeholders_t cuda_placeholders;

void 
cuda_memcpy
(
  void
)
{
  //TODO(Keren): extend state to copyin and copyout
}


void 
cuda_memalloc
(
  void
)
{
}


void 
cuda_kernel
(
  void
)
{
}


void
cuda_sync
(
 void
)
{
}



void
cuda_init_placeholders
(
)
{
  init_placeholder(&cuda_placeholders.cuda_memcpy_state, &cuda_memcpy);
  init_placeholder(&cuda_placeholders.cuda_memalloc_state, &cuda_memalloc);
  init_placeholder(&cuda_placeholders.cuda_kernel_state, &cuda_kernel);
  init_placeholder(&cuda_placeholders.cuda_sync_state, &cuda_sync);
}
