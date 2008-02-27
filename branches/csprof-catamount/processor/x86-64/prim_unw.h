// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef PRIM_UNW_H
#define PRIM_UNW_H

#include <ucontext.h>

#include "prim_unw_cursor.h"

extern void unw_init_context(ucontext_t* context, unw_cursor_t* cursor);
extern void unw_init_mcontext(mcontext_t* mctxt, unw_cursor_t* cursor);
extern int unw_get_reg(unw_cursor_t *c,int rid, void **reg);
extern void unw_init(void);

// Given a cursor, step the cursor to the next (less deeply nested)
// frame.  Conforms to the semantics of libunwind's unw_step.  In
// particular, returns:
//   > 0 : successfully advanced cursor to next frame
//     0 : previous frame was the end of the unwind
//   < 0 : error condition
extern int unw_step(unw_cursor_t *c);

typedef void *unw_word_t;

#define UNW_REG_IP 1

#endif
