#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

#include "splay-interval.h"

typedef struct _unw_c_t {
  void *pc;
  void **bp;
  void **sp;
  void *ra;
  splay_interval_t *intvl;
  int trolling_used;
} unw_cursor_t;

#endif
