#ifndef PRIM_UNW_H
#define PRIM_UNW_H

#include <ucontext.h>

#include "prim_unw_cursor.h"

extern void unw_init_context(ucontext_t* context, unw_cursor_t* cursor);
extern void unw_init_mcontext(mcontext_t* mctxt, unw_cursor_t* cursor);
extern int unw_get_reg(unw_cursor_t *c,int rid, void **reg);
extern void unw_init(void);
extern int unw_step(unw_cursor_t *c);
typedef void *unw_word_t;

#define UNW_REG_IP 1

#endif
