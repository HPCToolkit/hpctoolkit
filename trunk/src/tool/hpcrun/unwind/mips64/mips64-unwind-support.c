// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include "unwind.h"

//*************************** Forward Declarations **************************

#define MIPS_SP	29 /* stack pointer */
#define MIPS_FP	30 /* base pointer */
#define MIPS_RA	31 /* return address */

#define UCONTEXT_REG(uc, reg) (uc->uc_mcontext.gregs[reg])

#define UCONTEXT_PC(uc)       (void*)(uc->uc_mcontext.pc)
#define UCONTEXT_RA(uc)       (void*)(UCONTEXT_REG(uc, MIPS_RA))

#define UCONTEXT_BP(uc)       (void**)(UCONTEXT_REG(uc, MIPS_FP))
#define UCONTEXT_SP(uc)       (void**)(UCONTEXT_REG(uc, MIPS_SP))


//***************************************************************************
// interface functions
//***************************************************************************

void
unw_init_arch(void)
{
}


void*
context_pc(void* context)
{
  ucontext_t* uc = (ucontext_t*)context;
  return UCONTEXT_PC(uc);
}


void 
unw_init_cursor_arch(void* context, unw_cursor_t* cursor)
{
  ucontext_t* uc = (ucontext_t*)context;

  cursor->pc = UCONTEXT_PC(uc);
  cursor->bp = UCONTEXT_BP(uc);
  cursor->sp = UCONTEXT_SP(uc);
}


int 
unw_get_reg_arch(unw_cursor_t *cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;

  return 0;
}
