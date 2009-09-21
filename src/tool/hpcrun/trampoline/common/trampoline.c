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
// system includes
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "trampoline.h"
#include <hpcrun/thread_data.h>


//******************************************************************************
// external declarations
//******************************************************************************

extern void hpcrun_trampoline;
extern void hpcrun_trampoline_end;

//******************************************************************************
// local data and operations
//******************************************************************************

//
// save return addresses for trampolines in Thread Local Data
//

static void
save_retn_addr(void* addr)
{
  TD_GET(tramp_loc)       = addr;
  TD_GET(tramp_retn_addr) = *((void**) addr);
}

static void*
fetch_retn_addr(void)
{
  return TD_GET(tramp_retn_addr);
}

//******************************************************************************
// interface operations
//******************************************************************************


void
hpcrun_init_trampoline_info(void)
{
  thread_data_t* td   = csprof_get_thread_data();

  td->tramp_present   = false;
  td->tramp_retn_addr = NULL;
  td->tramp_loc       = NULL;
}

// returns 1 if address is in the assembly language trampoline code, else 0.
bool
hpcrun_addr_in_trampoline(void* addr)
{
    return (&hpcrun_trampoline <= addr && addr <= &hpcrun_trampoline_end);
}


void*
hpcrun_trampoline_handler(void)
{
  printf("trampoline fired!\n");
  void* ra = fetch_retn_addr();
  // After trampoline is invoked, it is effectively removed
  
  // FIXME: re-insert trampoline up the ladder

  return ra;
}


void 
hpcrun_trampoline_insert(void* addr)
{
  save_retn_addr(addr);
  *((void**)addr) = &hpcrun_trampoline;

  TD_GET(tramp_present) = true;
}


void 
hpcrun_trampoline_remove(void* addr, void* old_return_address)
{
  *((void**)addr) = old_return_address;
  TD_GET(tramp_present) = false;
}
