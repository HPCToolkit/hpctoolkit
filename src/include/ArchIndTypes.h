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

#ifdef NO_STD_CHEADERS
# include <limits.h>
#else
# include <climits>
#endif

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

#if ((__digital__ || __alpha) && __unix)
# define ALPHA_64 /* __arch64__ is not necessarily portable */  
#elif (__sgi && __mips && __unix)
# if ((_MIPS_ISA == _MIPS_ISA_MIPS1) || (_MIPS_ISA == _MIPS_ISA_MIPS2))
#  define MIPS_32
# elif ((_MIPS_ISA == _MIPS_ISA_MIPS3) || (_MIPS_ISA == _MIPS_ISA_MIPS4))
#  define MIPS_64
# else
#  error "ArchIndTypes - MIPS: does not know of your system"
# endif
#elif (__sun && __sparc && __unix)
# if (__sparcv9)
#  define SPARC_64
#  error "ArchIndTypes - SPARC: 64-bit arch has not been tested" 
# else // (__sparcv8)
#  define SPARC_32
# endif
#elif (__linux && __i386)
# define INTEL_IA32
#elif (__linux && __ia64)
# define INTEL_IA64
#elif (defined(__MACH__) && defined(__ppc__))
# define DARWIN_POWERPC
#else
# error "ArchIndTypes: does not know of your system" 
#endif

//****************************************************************************

// 64 and 32 bit compilation types (along with appropriate limits):
// - pointer-sized int/unit (psint, psuint)
// - standard-sized int/uint (sint, suint) 
//   Both are always guaranteed to have the same number of bits as a
//   pointer; the latter are designed for use as a type that is better
//   thought of as an integer (without the implementation/pointer
//   baggage, but where a few pointer casts are permissable).
// - 32-bit integer (int32, uint32)
//   For use as types where a certain size is needed independent of
//   the implementation.

#if ((__digital__ || __alpha) && __unix)

  // long/pointer: 64 bits; int: 32 bits
  typedef long          psint;  // pointer-sized int (64-bit)
  typedef unsigned long psuint; // pointer-sized unsigned int
# define PSINT_MIN  (LONG_MIN)
# define PSINT_MAX  (LONG_MAX)
# define PSUINT_MAX (ULONG_MAX)

  typedef int           int32;
  typedef unsigned int  uint32;
# define INT32_MIN  (INT_MIN)
# define INT32_MAX  (INT_MAX)
# define UINT32_MAX (UINT_MAX)

# define ARCH_INT_DIFFERS_FROM_SINT 1
  typedef long          int64;
  typedef unsigned long uint64;
#define INT64_MIN  (LONG_MIN)
#define INT64_MAX  (LONG_MAX)
#define UINT64_MAX (ULONG_MAX)

#elif (__sgi && __mips && __unix)
# if (_MIPS_SZLONG == 32)

   // int/long/pointer: 32 bits
   typedef int           psint;  // pointer-sized int (32-bit)
   typedef unsigned int  psuint; // pointer-sized unsigned int
#  define PSINT_MIN  (INT_MIN)
#  define PSINT_MAX  (INT_MAX)
#  define PSUINT_MAX (UINT_MAX)

   typedef int           int32;
   typedef unsigned int  uint32;
#  define INT32_MIN  (INT_MIN)
#  define INT32_MAX  (INT_MAX)
#  define UINT32_MAX (UINT_MAX)

# elif (_MIPS_SZLONG == 64)

   // long/pointer: 64 bits; int: 32 bits
   typedef long          psint;  // pointer-sized int (64-bit)
   typedef unsigned long psuint; // pointer-sized unsigned int
#  define PSINT_MIN  (LONG_MIN)
#  define PSINT_MAX  (LONG_MAX)
#  define PSUINT_MAX (ULONG_MAX)

   typedef int           int32;
   typedef unsigned int  uint32;
#  define INT32_MIN  (INT_MIN)
#  define INT32_MAX  (INT_MAX)
#  define UINT32_MAX (UINT_MAX)

#  define ARCH_INT_DIFFERS_FROM_SINT 1
   typedef long          int64;
   typedef unsigned long uint64;
#  define INT64_MIN  (LONG_MIN)
#  define INT64_MAX  (LONG_MAX)
#  define UINT64_MAX (ULONG_MAX)

# else
#  error "ArchIndTypes: Need a new type definition!"
# endif // __sgi
#elif (__sun && __sparc && __unix)
# if (__sparcv9)

   // long/pointer: 64 bits; int: 32 bits (_LP64)
   typedef long          psint;  // pointer-sized int (64-bit)
   typedef unsigned long psuint; // pointer-sized unsigned int
#  define PSINT_MIN  (LONG_MIN)
#  define PSINT_MAX  (LONG_MAX)
#  define PSUINT_MAX (ULONG_MAX)

   typedef int           int32;
   typedef unsigned int  uint32;
#  define INT32_MIN  (INT_MIN)
#  define INT32_MAX  (INT_MAX)
#  define UINT32_MAX (UINT_MAX)

#  error "ArchIndTypes: 64-bit sparc architecture has not been tested!"
#  define ARCH_INT_DIFFERS_FROM_SINT 1
   typedef long          int64;
   typedef unsigned long uint64;
#  define INT64_MIN  (LONG_MIN)
#  define INT64_MAX  (LONG_MAX)
#  define UINT64_MAX (ULONG_MAX)

# else //  (__sparcv8)

   // int/long/pointer: 32 bits (_ILP32)
   typedef long          psint;  // pointer-sized int (32-bit)
   typedef unsigned long psuint; // pointer-sized unsigned int
#  define PSINT_MIN  (LONG_MIN)
#  define PSINT_MAX  (LONG_MAX)
#  define PSUINT_MAX (ULONG_MAX)

   typedef int           int32;
   typedef unsigned int  uint32;
   // INT32_MIN, INT32_MAX, UINT32_MAX are already defined
#  define ARCH_USE_LONG_LONG 1

# endif // __sun
#elif (__i386 && __linux || DARWIN_POWERPC)

  // int/long/pointer: 32 bits (_ILP32)
  typedef long          psint;  // pointer-sized int (32-bit)
  typedef unsigned long psuint; // pointer-sized unsigned int
# define PSINT_MIN  (LONG_MIN)
# define PSINT_MAX  (LONG_MAX)
# define PSUINT_MAX (ULONG_MAX)

  typedef int           int32;
  typedef unsigned int  uint32;
# define INT32_MIN  (INT_MIN)
# define INT32_MAX  (INT_MAX)
# define UINT32_MAX (UINT_MAX)

#elif (__ia64 && __linux)

  // long/pointer: 64 bits; int: 32 bits
  typedef long          psint;  // pointer-sized int (64-bit)
  typedef unsigned long psuint; // pointer-sized unsigned int
# define PSINT_MIN  (LONG_MIN)
# define PSINT_MAX  (LONG_MAX)
# define PSUINT_MAX (ULONG_MAX)

  typedef int           int32;
  typedef unsigned int  uint32;
# define INT32_MIN  (INT_MIN)
# define INT32_MAX  (INT_MAX)
# define UINT32_MAX (UINT_MAX)

#else
# error "ArchIndTypes: Need a new type definition!"
#endif 


typedef psint  sint;   // standard sized int
typedef psuint suint;  // standard sized unsigned int
#define SINT_MIN  (PSINT_MIN)
#define SINT_MAX  (PSINT_MAX)
#define SUINT_MAX (PSUINT_MAX)

//****************************************************************************

typedef unsigned short ushort;
typedef unsigned long  ulong;

//****************************************************************************

#endif 
