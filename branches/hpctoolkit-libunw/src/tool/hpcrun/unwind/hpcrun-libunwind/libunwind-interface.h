#ifndef LIBUNWIND_INTERFACE_H
#define LIBUNWIND_INTERFACE_H

#include "unwind_cursor.h"

typedef void* unw_word_t;

// ----------------------------------------------------------
// unw_init
// ----------------------------------------------------------

void unw_init();


// ----------------------------------------------------------
// unw_get_reg
// ----------------------------------------------------------

typedef enum {
  UNW_REG_IP
} unw_regnum_t;

int unw_get_reg(unw_cursor_t *c, unw_regnum_t reg_id, unw_word_t* reg_value);

void* hpcrun_unw_get_ra_loc(unw_cursor_t* c);

// ----------------------------------------------------------
// unw_init_cursor
// ----------------------------------------------------------

void unw_init_cursor(unw_cursor_t* cursor, void* context);


// ----------------------------------------------------------
// unw_step: 
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame.  Conforms to the semantics of libunwind's
//   unw_step.  In particular, returns:
//     > 0 : successfully advanced cursor to next frame
//       0 : previous frame was the end of the unwind
//     < 0 : error condition
// ---------------------------------------------------------

int unw_step(unw_cursor_t *c);

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
void unw_throw(void);

#endif // LIBUNWIND_INTERFACE_H
