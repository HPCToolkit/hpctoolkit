#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

#include "intervals.h"

typedef struct _unw_c_t {
  void *pc;
  void **bp;
  void **sp;
  void *ra;
  unwind_interval *intvl;
  int trolling_used;
} unw_cursor_t;

#endif
