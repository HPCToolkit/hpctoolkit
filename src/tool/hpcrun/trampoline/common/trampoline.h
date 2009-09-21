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

extern void hpcrun_init_trampoline_info(void);

// returns 1 if address is in the assembly language trampoline code, else 0.
extern bool hpcrun_addr_in_trampoline(void* addr);

extern void* hpcrun_trampoline_handler(void);

extern void hpcrun_trampoline_insert(void* addr);

extern void hpcrun_trampoline_remove(void* addr, void* old_return_address);

#endif // trampoline_h

