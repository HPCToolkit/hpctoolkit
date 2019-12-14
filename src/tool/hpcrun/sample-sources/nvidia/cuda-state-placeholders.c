#include "cuda-state-placeholders.h"

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

cuda_placeholders_t cuda_placeholders;

void 
cuda_copy
(
  void
)
{
  //TODO(Keren): extend state to copyin and copyout
}


void 
cuda_alloc
(
  void
)
{
}


void 
cuda_delete
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
  cuda_placeholder_t *p, 
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
cuda_init_placeholders
(
)
{
  init_placeholder(&cuda_placeholders.cuda_copy_state, &cuda_copy);
  init_placeholder(&cuda_placeholders.cuda_alloc_state, &cuda_alloc);
  init_placeholder(&cuda_placeholders.cuda_delete_state, &cuda_delete);
  init_placeholder(&cuda_placeholders.cuda_kernel_state, &cuda_kernel);
  init_placeholder(&cuda_placeholders.cuda_sync_state, &cuda_sync);
}
