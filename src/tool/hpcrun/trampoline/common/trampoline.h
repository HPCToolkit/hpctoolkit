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

// *****************************************************************************
//    System Includes
// *****************************************************************************

#include <stdbool.h>


// *****************************************************************************
//    Local Includes
// *****************************************************************************

#include <hpcrun/thread_data.h>
#include <cct/cct.h>

// *****************************************************************************
//    Constants
// *****************************************************************************

enum trampoline_constants {
  CACHED_BACKTRACE_SIZE = 32
};

// *****************************************************************************
//    Interface Functions
// *****************************************************************************


void hpcrun_init_trampoline_info(void);

// returns true if address is in the assembly language trampoline code, else false.

bool hpcrun_trampoline_interior(void* addr);
bool hpcrun_trampoline_at_entry(void* addr);

void* hpcrun_trampoline_handler(void);

void hpcrun_trampoline_insert(csprof_cct_node_t* node);
void hpcrun_trampoline_remove(void);
csprof_cct_node_t* hpcrun_trampoline_advance(void);

#endif // trampoline_h
