// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef atomic_op_bodies_h
#define atomic_op_bodies_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

//***************************************************************************

// *** FIXME: tallent: ***
// 1. These platform switches should be based on the autoconf host (or
//    we should just use the gcc intrinsics).
// 2. If we keep them, we should just use a "int64_t" so we don't need
//    versions for each size of a "long".

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

#elif defined(__mips64)

  // ll r_dest, addr_offset(r_addr)
  // sc r_src,  addr_offset(r_addr) [sets r_src to 1 (success) or 0]

  // Note: lld/scd for 64 bit versions

#if (_MIPS_SIM == _ABI64)
#  define MIPS_WORD_SFX "d"
#elif (_MIPS_SIM == _ABIN32)
#  define MIPS_WORD_SFX ""
#else
#  error "Unknown MIPS platform!"
#endif

#define LL_BODY                      \
  __asm__ __volatile__(              \
        "ll"MIPS_WORD_SFX" %0,0(%1)" \
               	: "=r" (result)      \
               	: "r"(ptr))

#define SC_BODY                              \
  __asm__ __volatile__(                      \
       	"sc"MIPS_WORD_SFX" %2,0(%1) \n\t"    \
       	"move              %0,%2"            \
                : "=&r" (result)             \
               	: "r"(ptr), "r"(val)         \
               	: "memory")


#elif defined(__ppc64__)

  // lwarx r_dest, addr_offset, r_addr
  // stwcx r_src,  addr_offset, r_addr

#define LL_BODY                      \
  __asm__ __volatile__(              \
        "lwarx %0,0,%1"              \
               	: "=r" (result)      \
               	: "r"(ptr))

#define SC_BODY                      \
  __asm__ __volatile__(              \
       	"stwcx. %2,0,%1 \n\t"        \
       	"bne    $+12    \n\t"        \
       	"li     %0,1    \n\t"        \
       	"b      $+8     \n\t"        \
       	"li     %0,0"                \
       	        : "=&r" (result)     \
                : "r"(ptr), "r"(val) \
                : "cr0", "memory")
#else
#error "unknown processor"
#endif

#endif // atomic_op_bodies_h
