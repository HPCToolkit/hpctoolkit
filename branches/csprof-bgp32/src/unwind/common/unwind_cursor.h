#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

#include "intervals.h"

typedef struct _unw_c_t {
  void *pc;
  void **bp;
  void **sp;
  unwind_interval *intvl;
} unw_cursor_t;

#endif
