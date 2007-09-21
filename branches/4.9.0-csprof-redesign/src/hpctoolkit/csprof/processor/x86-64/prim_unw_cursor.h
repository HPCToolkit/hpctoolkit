#ifndef PRIM_UNW_CURSOR
#define PRIM_UNW_CURSOR
#include "intervals.h"
typedef struct _unw_c_t {
  void *pc;
  void **bp;
  void **sp;
  unwind_interval *intvl;
} unw_cursor_t;
#endif
