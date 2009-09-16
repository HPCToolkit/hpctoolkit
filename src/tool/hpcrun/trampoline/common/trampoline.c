//******************************************************************************
// File: trampoline.c
//
// Purpose: architecture independent support for counting returns of sampled
//          frames using trampolines
//
// Modification History:
//   2009/09/15 - created - Mike Fagan and John Mellor-Crummey
//******************************************************************************

//******************************************************************************
// local includes
//******************************************************************************

#include "trampoline.h"



//******************************************************************************
// external declarations
//******************************************************************************

extern void hpcrun_trampoline;
extern void hpcrun_trampoline_end;



//******************************************************************************
// interface operations
//******************************************************************************


// returns 1 if address is in the assembly language trampoline code, else 0.
int
hpcrun_addr_in_trampoline(void *addr)
{
    return (&hpcrun_trampoline <= addr && addr <= &hpcrun_trampoline_end);
}


void *
hpcrun_trampoline_handler()
{
  void *return_address;

  return return_address;
}


void 
hpcrun_trampoline_insert(void **addr)
{
  *addr = &hpcrun_trampoline;
}


void 
hpcrun_trampoline_remove(void **addr, void *old_return_address)
{
  *addr = old_return_address;
}
