#ifndef trampoline_h
#define trampoline_h

//******************************************************************************
// File: trampoline.h
//
// Purpose: architecture independent support for counting returns of sampled
//          frames using trampolines
//
// Modification History:
//   2009/09/15 - created - Mike Fagan and John Mellor-Crummey
//******************************************************************************

#include <stdbool.h>

void hpcrun_init_trampoline_info(void);

// returns true if address is in the assembly language trampoline code, else false.

bool hpcrun_trampoline_interior(void* addr);
bool hpcrun_trampoline_at_entry(void* addr);

void* hpcrun_trampoline_handler(void);

void hpcrun_trampoline_insert(void* addr);
void hpcrun_trampoline_remove(void* addr, void* old_return_address);

#endif // trampoline_h
