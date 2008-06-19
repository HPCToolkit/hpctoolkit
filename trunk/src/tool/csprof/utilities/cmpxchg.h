#ifndef cmpxchg_h
#define cmpxchg_h

#include "atomic-ops.h"

static inline unsigned long
cmpxchg(volatile void *ptr, unsigned long old, unsigned long new)
{
  return compare_and_swap(ptr, old, new);
}

#endif // cmpxchg_h
