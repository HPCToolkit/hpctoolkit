#ifndef atomic_h
#define atomic_h

#include "atomic-ops.h"

#include <include/gcc-attr.h>


static inline long
csprof_atomic_increment(volatile long *addr)
{
    long old = fetch_and_add(addr, 1);
    return old;
}


static inline long
csprof_atomic_decrement(volatile long *addr)
{
    long old = fetch_and_add(addr, -1);
    return old;
}


static inline long
csprof_atomic_swap_l(volatile long *addr, long new)
{
    long old = fetch_and_store(addr, new);
    return old;
}


#define csprof_atomic_swap_p(vaddr, ptr) ((void *) csprof_atomic_swap_l((volatile long *)vaddr, (long) ptr))


// FIXME:tallent: this should be replace the "csprof" routines
#if (GCC_VERSION >= 4100)
#  define hpcrun_atomicIncr(x)      (void)__sync_add_and_fetch(x, 1)
#  define hpcrun_atomicDecr(x)      (void)__sync_sub_and_fetch(x, 1)
#  define hpcrun_atomicAdd(x, val)  (void)__sync_add_and_fetch(x, val)
#else
#  warning "atomic.h: using slow atomics!"
#  define hpcrun_atomicIncr(x)      csprof_atomic_increment(x)
#  define hpcrun_atomicDecr(x)      csprof_atomic_decrement(x)
#  define hpcrun_atomicAdd(x, val)  fetch_and_add(x, val)
#endif


#endif // atomic_h
