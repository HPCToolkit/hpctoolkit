#ifndef atomic_h
#define atomic_h

#include "atomic-ops.h"

/* FIXME: atomic_increment and atomic_decrement could be implemented more
   efficiently on x86-64 and x86. 

   See lush/lush-pthread.h
 */


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

#endif // atomic_h
