// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef UNWIND_H
#define UNWIND_H

#include <ucontext.h>

#include "unwind_cursor.h"


extern void unw_init(void);

extern void unw_init_cursor(ucontext_t* context, unw_cursor_t* cursor);

// Given a cursor, step the cursor to the next (less deeply nested)
// frame.  Conforms to the semantics of libunwind's unw_step.  In
// particular, returns:
//   > 0 : successfully advanced cursor to next frame
//     0 : previous frame was the end of the unwind
//   < 0 : error condition
extern int unw_step(unw_cursor_t *c);

extern int unw_get_reg(unw_cursor_t *c,int rid, void **reg);

typedef void *unw_word_t;

#define UNW_REG_IP 1

#endif
