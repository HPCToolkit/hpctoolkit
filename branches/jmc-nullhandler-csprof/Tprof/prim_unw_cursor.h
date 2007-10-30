#ifndef PRIM_UNW_CURSOR
#define PRIM_UNW_CURSOR
typedef struct _unw_c_t {
  void *pc;
  void **bp;
} unw_cursor_t;
#endif
