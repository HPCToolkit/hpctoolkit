#ifndef CSPROF_ATOMIC_H
#define CSPROF_ATOMIC_H

/* various atomic instructions; we use these in lieu of pthread-provided
   locks because locks and signals don't work too well together */

#if defined(__osf__) && defined(__DECC)

/* Digital^WCompaq^WHP is nice enough to include these builtin */
#include <machine/builtins.h>

static inline long
csprof_atomic_increment(volatile void *addr)
{
    return __ATOMIC_INCREMENT_LONG(addr);
}

static inline long
csprof_atomic_decrement(volatile void *addr)
{
    return __ATOMIC_DECREMENT_LONG(addr);
}

static inline void *
csprof_atomic_exchange_pointer(volatile void **addr, void *ptr)
{
    return (void *)__ATOMIC_EXCH_QUAD(addr, ptr);
}

#endif /* __osf__ && __DECC */

#if defined(__i386__) && defined(__linux__)

/* FIXME: these are *probably* broken; they've never been tested.  */
static inline long
csprof_atomic_increment(volatile void *addr)
{
    int prev;

    asm volatile
        ("\n"
         "1:\n\t"
         "movl %1, %0\n\t"
         "movl %0, edx\n\t"
         "movl %0, eax\n\t"
         "incl edx\n\t"
         "lock; cmpxchg %1, edx\n\t"
         "cmpl eax, edx\n\t"
         "je 1b\n\t"
         : "=r" (prev) : "m" (addr) : "eax", "edx");

    return prev;
}

static inline long
csprof_atomic_decrement(volatile void *addr)
{
    int prev;

    asm volatile
        ("\n"
         "1:\n\t"
         "movl %1, %0\n\t"
         "movl %0, edx\n\t"
         "movl %0, eax\n\t"
         "decl edx\n\t"
         "lock; cmpxchg %1, edx\n\t"
         "cmpl eax, edx\n\t"
         "je 1b\n\t"
         : "=r" (prev) : "m" (addr) : "eax", "edx");

    return prev;
}

static inline void *
csprof_atomic_exchange_pointer(volatile void **addr, volatile void *ptr)
{
    void *prev;

    asm volatile
        ("\n"
         "1:\n\t"
         "movl %2, %0\n\t"
         "movl %0, eax\n\t"
         "movl %1, edx\n\t"
         "lock; cmpxchg %2, edx\n\t"
         "cmpl eax, edx\n\t"
         "je 1b\n\t"
         : "=r" (prev) : "r" (ptr), "m" (addr) : "eax", "edx");

    return prev;
}

#endif /* __i386__ && __linux__ */

#if defined(__linux__)

#if defined(__x86_64__)

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

#endif /* __linux__ */

#endif
