#include "gpu-driver-state-placeholders.h"

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

gpu_driver_placeholders_t gpu_driver_placeholders;


void 
gpu_driver_copy
(
  void
)
{
}


void 
gpu_driver_copyin
(
  void
)
{
}


void 
gpu_driver_copyout
(
  void
)
{
}


void 
gpu_driver_alloc
(
  void
)
{
}


void
gpu_driver_delete
(
 void
)
{
}


void
gpu_driver_sync
(
 void
)
{
}


void
gpu_driver_kernel
(
 void
)
{
}


void
gpu_driver_trace
(
 void
)
{
}


static load_module_t *
pc_to_lm
(
  void *pc
)
{
  void *func_start_pc, *func_end_pc;
  load_module_t *lm = NULL;
  fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc, &lm);
  return lm;
}


static void 
init_placeholder
(
  gpu_driver_placeholder_t *p, 
  void *pc
)
{
  // protect against receiving a sample here. if we do, we may get 
  // deadlock trying to acquire a lock associated with 
  // fnbounds_enclosing_addr
  hpcrun_safe_enter();
  {
    void *cpc = canonicalize_placeholder(pc);
    p->pc = cpc;
    p->pc_norm = hpcrun_normalize_ip(cpc, pc_to_lm(cpc));
  }
  hpcrun_safe_exit();
}


void
gpu_driver_init_placeholders
(
)
{
  init_placeholder(&gpu_driver_placeholders.gpu_copy_state, &gpu_driver_copy);
  init_placeholder(&gpu_driver_placeholders.gpu_copyin_state, &gpu_driver_copyin);
  init_placeholder(&gpu_driver_placeholders.gpu_copyout_state, &gpu_driver_copyout);
  init_placeholder(&gpu_driver_placeholders.gpu_alloc_state, &gpu_driver_alloc);
  init_placeholder(&gpu_driver_placeholders.gpu_delete_state, &gpu_driver_delete);
  init_placeholder(&gpu_driver_placeholders.gpu_sync_state, &gpu_driver_sync);
  init_placeholder(&gpu_driver_placeholders.gpu_kernel_state, &gpu_driver_kernel);
  init_placeholder(&gpu_driver_placeholders.gpu_trace_state, &gpu_driver_trace);
}
