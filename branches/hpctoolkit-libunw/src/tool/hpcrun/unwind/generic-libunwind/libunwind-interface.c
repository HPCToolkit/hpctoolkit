//***************************************************************************
//
// File:
//   libunwind-interface.c
//
// Purpose:
//
//  Given a 
//  Implement the remaining hpcrun unwind functionality.
//
//  The functions:
//    unw_step
//    unw_get_reg
//
// are supplied by generic libunwind.
//
// 
//***************************************************************************

#include "libunwind-interface.h"

// ----------------------------------------------------------
// unw_init
// ----------------------------------------------------------

void unw_init()
{
}


void* hpcrun_unw_get_ra_loc(unw_cursor_t* c)
{
}

// ----------------------------------------------------------
// unw_init_cursor
// ----------------------------------------------------------

void unw_init_cursor(unw_cursor_t* cursor, void* context)
{
}

//***************************************************************************
//
// services provided by HPCToolkit's unwinder
//
//   these routines are called by the common backtrace routines and
//   therefore must be provided by architecture-specific unwind
//   agents.
//
//***************************************************************************

// ----------------------------------------------------------
// unw_troll_stack
// ----------------------------------------------------------

// FIXME: tallent: move stack trolling code here


// ----------------------------------------------------------
// unw_throw:
// ----------------------------------------------------------

// FIXME: tallent: the code in x86-unwind.c probably should be common
void unw_throw(void)
{
}

#endif // LIBUNWIND_INTERFACE_H
