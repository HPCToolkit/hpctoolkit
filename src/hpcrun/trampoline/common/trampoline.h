// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#include "../../thread_data.h"
#include "../../cct/cct.h"

// *****************************************************************************
//    Constants
// *****************************************************************************

enum trampoline_constants {
  CACHED_BACKTRACE_SIZE = 32
};

// *****************************************************************************
//    Interface Functions
// *****************************************************************************


extern void hpcrun_init_trampoline_info(void);

// returns true if address is in the assembly language trampoline code, else false.

extern bool hpcrun_trampoline_interior(void* addr);
extern bool hpcrun_trampoline_at_entry(void* addr);

extern void* hpcrun_trampoline_handler(void);

extern void hpcrun_trampoline_insert(cct_node_t* node);
extern void hpcrun_trampoline_remove(void);
extern void hpcrun_trampoline_bt_dump(void);

extern bool hpcrun_trampoline_update(frame_t* stop_frame);

extern void hpcrun_trampoline(void);
extern void hpcrun_trampoline_end(void);
#endif // trampoline_h
