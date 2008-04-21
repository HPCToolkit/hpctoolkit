#ifndef atomic_h
#define atomic_h

#include "cmpxchg.h"

/* FIXME: atomic_increment and atomic_decrement could be implemented more
   efficiently on x86-64 and x86.  */


static inline long
csprof_atomic_increment(volatile long *addr)
{
    long old, new;

    do {
        old = *addr;
        new = old + 1;
    } while (cmpxchg(addr, old, new) != old);

    return old;
}


static inline long
csprof_atomic_decrement(volatile long *addr)
{
    long old, new;

    do {
      old = *addr;
      new = old - 1;
    } while(cmpxchg(addr, old, new) != old);

    return old;
}


static inline long
csprof_atomic_swap_l(volatile long *addr, long new)
{
    long old;
    do {
      old = *addr;
    } while(cmpxchg(addr, old, new) != old);
    return old;
}


#define csprof_atomic_swap_p(vaddr, ptr) ((void *) csprof_atomic_swap_l((volatile long *)vaddr, (long) ptr))

#endif // atomic_h
