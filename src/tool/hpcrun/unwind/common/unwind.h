// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
// File:
//   unwind.h
//
// Purpose:
//   interface to the stack-unwinding primitives. 
//
//***************************************************************************

#ifndef unwind_h
#define unwind_h

//***************************************************************************
// system include files
//***************************************************************************

#include <ucontext.h>


//***************************************************************************
// local include files
//***************************************************************************

#include "unwind_cursor.h"


//***************************************************************************
// interface to HPCToolkit's unwinder (cf. libunwind)
//***************************************************************************

typedef void* unw_word_t;

// ----------------------------------------------------------
// unw_init
// ----------------------------------------------------------

extern void unw_init();


// ----------------------------------------------------------
// unw_init_cursor
// ----------------------------------------------------------

// FIXME: tallent: cursor should be the first argument (consistent
// with libunwind and a signal that it is modified).
extern void unw_init_cursor(void* context, unw_cursor_t* cursor);


// ----------------------------------------------------------
// unw_get_reg
// ----------------------------------------------------------

#define UNW_REG_IP 1

extern int unw_get_reg(unw_cursor_t *c, int reg_id, void **reg_value);


// ----------------------------------------------------------
// unw_step: 
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame.  Conforms to the semantics of libunwind's
//   unw_step.  In particular, returns:
//     > 0 : successfully advanced cursor to next frame
//       0 : previous frame was the end of the unwind
//     < 0 : error condition
// ---------------------------------------------------------

typedef enum {
  STEP_ERROR = -1,
  STEP_STOP  = 0,
  STEP_OK    = 1,
  STEP_TROLL = 2,
} step_state;

extern int unw_step(unw_cursor_t *c);


// ----------------------------------------------------------
// unw_throw
// ----------------------------------------------------------

// FIXME: tallent: the code in x86-unwind.c probably should be common
extern void unw_throw();


//***************************************************************************

// FIXME: tallent: this probably does not need to be exposed
void *context_pc(void *context);


//***************************************************************************

#endif // unwind_h
