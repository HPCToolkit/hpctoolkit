/******************************************************************************
 * include files
 *****************************************************************************/

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"
#include <lib/isa-lean/power/instruction-set.h>

static inline bool
isInsn_B(uint32_t insn, uint32_t *target, char* addr)
{
  if((insn & PPC_INSN_I_MASK & PPC_OP_I_MASK) == PPC_OP_B) {
    *target =(uint32_t)(PPC_OPND_LI(insn) + addr);
    return true;
  }
  return false;
}

void *tailcall_target(void *addr, long offset)
{
  uint32_t target = 0;
  if(isInsn_B(*((uint32_t *)((char *)addr+offset)), &target, (char *)addr)) {
    return (void *)target;
  }
  return NULL;
}

void
process_range_init(void)
{
  return;
}


void
process_range(long offset, void *vstart, void *vend, DiscoverFnTy fn_discovery)
{
  return;
}


bool
range_contains_control_flow(void *vstart, void *vend)
{
  return true;
}

