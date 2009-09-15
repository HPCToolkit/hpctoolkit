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

int hpcrun_addr_in_trampoline(void *addr);

#endif // trampoline_h

