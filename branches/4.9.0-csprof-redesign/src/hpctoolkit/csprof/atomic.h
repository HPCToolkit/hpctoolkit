#ifndef CSPROF_ATOMIC_H
#define CSPROF_ATOMIC_H

/* various atomic instructions; we use these in lieu of pthread-provided
   locks because locks and signals don't work too well together */

#if defined(__i386__) 

static inline long
cmpxchg(volatile void *ptr, long old, long new)
{
  long prev;

  __asm__ __volatile__("\n"
		       "\tlock; cmpxchg %3, (%1)\n\t"
		       : "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory");

  return prev;
}

#elif defined(__x86_64__)

static inline long
cmpxchg(volatile void *ptr, long old, long new)
{
  long prev;

  __asm__ __volatile__("\n"
		       "\tlock; cmpxchgq %3, (%1)\n\t"
		       : "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory");

  return prev;
}

#elif defined(__ia64__)

static inline long
cmpxchg(volatile void *ptr, long old, long new)
{
    long _o, _r;
    _o = old;

    __asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO"(_o));
    __asm__ __volatile__ ("cmpxchg8.acq %0=[%1],%2,ar.ccv"
                          : "=r"(_r) : "r"(ptr), "r"(new) : "memory");

    return _r;
}
#else
#error "unknown processor"
#endif

/* FIXME: atomic_increment and atomic_decrement could be implemented more
   efficiently on x86-64 and x86.  */
static inline long
csprof_atomic_increment(volatile void *addr)
{
    long old, new;

    do {
        old = *((long *)addr);
        new = old + 1;
    } while(cmpxchg(addr, old, new) != old);

    return old;
}

static inline long
csprof_atomic_decrement(volatile void *addr)
{
    long old, new;

    do {
        old = *((long *)addr);
        new = old - 1;
    } while(cmpxchg(addr, old, new) != old);

    return old;
}

static inline void *
csprof_atomic_exchange_pointer(volatile void **addr, volatile void *ptr)
{
    void *old = (void *)*addr;

    while(cmpxchg(addr, (long) old, (long) ptr) != (long) old);

    return old;
}


static inline void *
csprof_atomic_exchange_long(volatile long *addr, long newval)
{
    long old = *addr;

    while(cmpxchg(addr, old, newval) != old);

    return (void *)old;
}

#endif
