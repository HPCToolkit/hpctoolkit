#ifndef PRIM_UNW_H
#define PRIM_UNW_H
#include "prim_unw_cursor.h"
extern int unw_get_reg(unw_cursor_t *c,int rid, void **reg);
extern int unw_step(unw_cursor_t *c);
#endif
