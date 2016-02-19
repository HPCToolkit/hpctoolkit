// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File: 
//   $HeadURL$
//
// Purpose:
//   Atomics.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   [...]
//
//***************************************************************************

#ifndef prof_lean_atomic_op_i
#define prof_lean_atomic_op_i

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

//***************************************************************************

// *** FIXME: tallent: ***
// 1. These platform switches should be based on the autoconf host (or
//    we should just use the gcc intrinsics).
// 2. We should also replace them with versions that support different types

#if defined(__i386__) 

#define CAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\tlock; cmpxchg %3, (%1)\n\t"                               \
		       : "=a" (prev) : "r" (ptr), "a" (oldval), "r" (newval) : "memory") 

#define FAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\txchg %2, (%1)\n\t"                                        \
		       : "=a" (prev) : "r" (ptr), "a" (newval) : "memory") 

#elif defined(__x86_64__)

#define CAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\tlock; cmpxchgq %3, (%1)\n\t"                              \
		       : "=a" (prev) : "r" (ptr), "a" (oldval), "r" (newval) : "memory")

#define CAS4_BODY                                                                   \
  __asm__ __volatile__("\n"                                                         \
		       "\tlock; cmpxchgl %3, (%1)\n\t"                              \
		       : "=a" (prev) : "r" (ptr), "a" (oldval), "r" (newval) : "memory")

#define FAS_BODY                                                                    \
  __asm__ __volatile__("\n"                                                         \
		       "\txchg %2, (%1)\n\t"                                        \
		       : "=a" (prev) : "r" (ptr), "a" (newval) : "memory") 

#elif defined(__ia64__)

#define CAS_BODY {                                                                  \
    long __o = oldval;                                                              \
    __asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO"(__o));                          \
    __asm__ __volatile__ ("cmpxchg8.acq %0=[%1],%2,ar.ccv"                          \
                          : "=r"(prev) : "r"(ptr), "r"(newval) : "memory");         \
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

#elif defined(__powerpc64__)

  // 64-bit powerpc
  // ldarx r_dest, addr_offset, r_addr
  // stdcx r_src,  addr_offset, r_addr

#define LL_BODY                      \
  __asm__ __volatile__(              \
        "ldarx %0,0,%1"              \
               	: "=r" (result)      \
               	: "r"(ptr))

#define SC_BODY                      \
  __asm__ __volatile__(              \
       	"stdcx. %2,0,%1 \n\t"        \
       	"bne    $+12    \n\t"        \
       	"li     %0,1    \n\t"        \
       	"b      $+8     \n\t"        \
       	"li     %0,0"                \
       	        : "=&r" (result)     \
                : "r"(ptr), "r"(val) \
                : "cr0", "memory")

#elif defined(__powerpc__)

  // 32-bit powerpc
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

#endif // prof_lean_atomic_op_i
