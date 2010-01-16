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

//************************************************
// system includes
//************************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

//************************************************
//  local includes
//************************************************

#include "libunwind-interface.h"

//
// external declarations
//

void hpcrun_drop_sample(void);
void hpcrun_interval_tree_init(void);

//************************************************
//  interface functions
//************************************************

//
// FIXME: context_pc function is separate from libunwind
//  context_pc is specific to an architecture/os pair.
//  Should likely be in it's own file.
//
//  the function below is for ia64 on linux
//

void*
context_pc(void* context)
{
  mcontext_t* mc = &((((ucontext_t*) context)->_u)._mc);
  return (void*) mc->sc_ip;
}

// ----------------------------------------------------------
// unw_init
// ----------------------------------------------------------

void
unw_init(void)
{
  hpcrun_interval_tree_init();
}


void*
hpcrun_unw_get_ra_loc(unw_cursor_t* c)
{
  return NULL;
}

// ----------------------------------------------------------
// unw_init_cursor
// ----------------------------------------------------------

void
unw_init_cursor(unw_cursor_t* cursor, void* context)
{
  unw_init_local(cursor, (unw_context_t*) context);
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
  hpcrun_drop_sample();
}
