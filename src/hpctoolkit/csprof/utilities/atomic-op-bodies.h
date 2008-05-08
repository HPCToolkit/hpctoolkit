#ifndef atomic_op_bodies_h
#define atomic_op_bodies_h

#if defined(__i386__) 

#define CAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\tlock; cmpxchg %3, (%1)\n\t"                               \
		       : "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory") 

#elif defined(__x86_64__)

#define CAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\tlock; cmpxchgq %3, (%1)\n\t"                              \
		       : "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory")

#elif defined(__ia64__)

#define CAS_BODY {                                                                  \
    long __o = old;                                                                 \
    __asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO"(__o));                          \
    __asm__ __volatile__ ("cmpxchg8.acq %0=[%1],%2,ar.ccv"                          \
                          : "=r"(prev) : "r"(ptr), "r"(new) : "memory");            \
}


#elif 1 
// defined(__ppc64__)

#define LL_BODY \
   __asm__ __volatile__(                                                            \
       "   lwarx %0,0,%1"                                                           \
               : "=r" (result)                                                      \
               : "r"(ptr))

#define SC_BODY \
   __asm__ __volatile__(                                                            \
       "   stwcx. %2,0,%1          \n"                                              \
       "   bne    $+12             \n"                                              \
       "   li     %0,1             \n"                                              \
       "   b      $+8              \n"                                              \
       "   li     %0,0"                                                             \
               : "=&r" (result)                                                     \
               : "r"(ptr), "r"(val)                                                 \
               : "cr0", "memory")
#else
#error "unknown processor"
#endif

#endif // atomic_op_bodies_h
