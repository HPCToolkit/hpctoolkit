#ifndef cmpxchg_h
#define cmpxchg_h

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

#endif // cmpxchg_h
