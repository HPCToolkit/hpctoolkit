#ifndef GENERIC_CONCURRENT_STACK_ENTRY_DATA_H
#define GENERIC_CONCURRENT_STACK_ENTRY_DATA_H 1

#include <stdatomic.h>

#define CONCURRENT_STACK_ENTRY_DATA(ENTRY_TYPE)     \
struct {                                            \
  _Atomic(ENTRY_TYPE *) next;                       \
}

#endif
