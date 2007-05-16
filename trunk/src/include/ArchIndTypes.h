// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#ifndef _include_ArchIndTypes_
#define _include_ArchIndTypes_

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

// TODO: Push these tests to configure.ac

// Verify that we are using a known platform and define platform and
// architecture macros.
// (Note: These are in alphabetical order by platform name)

// To see what macros gcc-like compilers define:
//   gcc -dM -E - < /dev/null | sort

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
# define OS_LINUX
# if (_MIPS_SIM == _ABIN32)
#  define ARCH_MIPS32
#  define ARCH_32
# elif (_MIPS_SIM == _ABI64)
#  define ARCH_MIPS64
#  define ARCH_64
# else
#  error "ArchIndTypes.h: Unknown MIPS/Linux platform."
# endif

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

// Special types for convenience 
//   - Analagous to historical ushort, ulong

#if defined(__cplusplus) 
  // This can still cause duplicate definition conflicts (with system
  // headers) in some C code.  Eventually we will be able to rely on
  // C99 conformance.
  typedef    unsigned short int    ushort;
  typedef    unsigned       int    uint;
  typedef    unsigned long  int    ulong;
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

#endif /* _include_ArchIndTypes_ */
