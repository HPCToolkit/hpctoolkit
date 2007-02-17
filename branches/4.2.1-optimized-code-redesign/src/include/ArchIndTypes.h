// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    ArchIndTypes.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ArchIndTypes_H
#define ArchIndTypes_H

//************************* System Include Files ****************************

// The following provides a portable definition of basic types
#include <inttypes.h> /* commonly available, unlike <stdint.h> */

#if defined(__cplusplus)
# ifdef NO_STD_CHEADERS
#  include <limits.h>
# else
#  include <climits>
# endif
#else
# include <limits.h>
#endif

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

//****************************************************************************

// Verify that we are using a known platform and define platform and
// architecture macros.
// (Note: These are in alphabetical order by platform name)

#if ((defined(__alpha) || defined(__digital__)) && defined(__unix__))
# define PLATFORM_ALPHA_OSF1
# define ARCH_ALPHA64
# define ARCH_64
# define OS_OSF1

#elif (defined(__i386) && defined(__linux))
# define PLATFORM_I686_LINUX
# define ARCH_IA32
# define ARCH_32
# define OS_LINUX

#elif (defined(__ia64) && defined(__linux))
# define PLATFORM_IA64_LINUX
# define ARCH_IA64
# define ARCH_64
# define OS_LINUX

#elif (defined(__mips) && defined(__sgi) && defined(__unix))
# define PLATFORM_MIPS_IRIX64
# define OS_IRIX64
# if ((_MIPS_ISA == _MIPS_ISA_MIPS1) || (_MIPS_ISA == _MIPS_ISA_MIPS2))
#  define ARCH_MIPS32
#  define ARCH_32
# elif ((_MIPS_ISA == _MIPS_ISA_MIPS3) || (_MIPS_ISA == _MIPS_ISA_MIPS4))
#  define ARCH_MIPS64
#  define ARCH_64
# else
#  error "ArchIndTypes.h: Unknown MIPS/IRIX platform."
# endif

#elif (defined(__mips64) && defined(__linux))
# define PLATFORM_MIPS64_LINUX
# define ARCH_MIPS64
# define ARCH_64
/*# define ARCH_MIPS32 * -n32 */
/*# define ARCH_32     * -n32 */
# define OS_LINUX

#elif (defined(__x86_64) || defined(__x86_64__))
# define PLATFORM_X86_64_LINUX
# define ARCH_X86_64
# define ARCH_64
# define OS_LINUX

#elif (defined(__ppc__) && defined(__MACH__))
# define PLATFORM_POWERPC_DARWIN
# define ARCH_POWERPC32
# define ARCH_32
# define OS_MACHDARWIN

#elif (defined(__sparc) && defined(__sun) && defined(__unix))
# define PLATFORM_SPARC_SUNOS
# define OS_SUNOS
# if (defined(__sparcv9))
#  define ARCH_SPARC64
#  define ARCH_64
#  error "ArchIndTypes.h: Sparc64 architecture has not been tested."
# else // (__sparcv8)
#  define ARCH_SPARC32
#  define ARCH_32
# endif

#else
# error "ArchIndTypes.h does not know of your platform!"
#endif

//****************************************************************************

// Special types (along with appropriate limits):
// - pointer-sized integer: psint, psuint
// - standard-sized integer: sint, suint
//   Both are always guaranteed to have the same number of bits as a
//   pointer; the latter are designed for use as a type that is better
//   thought of as an integer (without the implementation/pointer
//   baggage, but where a few pointer casts are permissable).

#if (defined(ARCH_IA32) || defined(ARCH_MIPS32) || defined(ARCH_POWERPC32) \
     || defined(ARCH_SPARC32))

  // int/long/pointer: 32 bits
  typedef int           psint;  // pointer-sized int (32-bit)
  typedef unsigned int  psuint; // pointer-sized unsigned int
# define PSINT_MIN  (INT_MIN)
# define PSINT_MAX  (INT_MAX)
# define PSUINT_MAX (UINT_MAX)

#elif (defined(ARCH_ALPHA64) || defined(ARCH_IA64) || defined(ARCH_MIPS64) \
       || defined(ARCH_SPARC64)|| defined(ARCH_X86_64))

  // long/pointer: 64 bits; int: 32 bits
  typedef long          psint;  // pointer-sized int (64-bit)
  typedef unsigned long psuint; // pointer-sized unsigned int
# define PSINT_MIN  (LONG_MIN)
# define PSINT_MAX  (LONG_MAX)
# define PSUINT_MAX (ULONG_MAX)

# define ARCH_INT_DIFFERS_FROM_SINT 1

#else
# error "ArchIndTypes.h: Internal error defining special types."
#endif 


typedef psint  sint;   // standard sized int
typedef psuint suint;  // standard sized unsigned int
#define SINT_MIN  (PSINT_MIN)
#define SINT_MAX  (PSINT_MAX)
#define SUINT_MAX (PSUINT_MAX)


// Special types for convenience 
//   - Analagous to historical ushort, ulong

#if defined(__cplusplus) 
  // This can still cause duplicate definition conflicts (with system
  // headers) in some C code.  Eventually we will be able to rely on
  // C99 conformance.
  typedef unsigned short int ushort;
  typedef unsigned long  int ulong;
#endif

//****************************************************************************

// Macros for integer literal expressions
// (Analagous to INTx_C() on Solaris/Linux, provided by inttypes.h)

#ifndef INT8_C
# define INT8_C(x)   x
#endif
#ifndef UINT8_C
# define UINT8_C(x)  x ## U
#endif
#ifndef INT16_C
# define INT16_C(x)  x
#endif
#ifndef UINT16_C
# define UINT16_C(x) x ## U
#endif

#if defined(ARCH_32)
# ifndef INT32_C /* use as a synecdoche for INT32_C and INT64_C */
#  define INT32_C(x)  x ## L
#  define UINT32_C(x) x ## UL
#  define INT64_C(x)  x ## LL
#  define UINT64_C(x) x ## ULL
# endif
#elif defined(ARCH_64)
# ifndef INT32_C /* use as a synecdoche for xINT32_C and xINT64_C */
#  define INT32_C(x)  x
#  define UINT32_C(x) x
#  define INT64_C(x)  x ## L
#  define UINT64_C(x) x ## UL
# endif
#else
# error "ArchIndTypes.h: Internal error defining int literal macros."
#endif

//****************************************************************************

// printf format strings for constant sized types
// (Analagous to PRIab() on Solaris/Linux, provided by inttypes.h)

#if defined(ARCH_32)
# ifndef PRId64 /* use as a synecdoche for PRIa64 */
#  define PRId64    "lld"
#  define PRIu64    "llu"
#  define PRIx64    "llx"
# endif
# ifndef SCNd64 /* use as a synecdoche for SCNa64 */
#  define SCNd64    "lld"
#  define SCNu64    "llu"
#  define SCNx64    "llx"
# endif
#elif defined(ARCH_64)
# ifndef PRId64 /* use as a synecdoche for PRIa64 */
#  define PRId64    "ld"
#  define PRIu64    "lu"
#  define PRIx64    "lx"
# endif
# ifndef SCNd64 /* use as a synecdoche for SCNa64 */
#  define SCNd64    "ld"
#  define SCNu64    "lu"
#  define SCNx64    "lx"
# endif
#else
# error "ArchIndTypes.h: Internal error defining printf macros."
#endif

//****************************************************************************

#endif 
