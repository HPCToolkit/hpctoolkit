/* -*-Mode: C-*- */
/* $Id$ */

/* * BeginRiceCopyright *****************************************************
 * 
 * Copyright ((c)) 2002-2007, Rice University 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of Rice University (RICE) nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * This software is provided by RICE and contributors "as is" and any
 * express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular
 * purpose are disclaimed. In no event shall RICE or contributors be
 * liable for any direct, indirect, incidental, special, exemplary, or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or
 * business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence
 * or otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage. 
 * 
 * ******************************************************* EndRiceCopyright */

/****************************************************************************
 *
 * File:
 *    general.h
 *
 * Purpose:
 *    [The purpose of this file]
 *
 * Description:
 *    [The set of functions, macros, etc. defined in the file]
 *
 ****************************************************************************/

#ifndef include_general_h
#define include_general_h

/****************************************************************************/

#if !defined(SIZEOF_VOIDP)
# error "configure.ac should have defined SIZEOF_VOIDP using AC_CHECK_SIZEOF."
#endif

/****************************************************************************/

/* --------------------------------------------------------------------------
 * NULL
 * ------------------------------------------------------------------------*/

#if defined(__cplusplus)
# ifdef NO_STD_CHEADERS
#  include <stddef.h>
# else
#  include <cstddef> // for 'NULL'
# endif
#else
#  include <stddef.h>
#endif


/* --------------------------------------------------------------------------
 * Convenient types (Possibly push detection to configure.ac)
 * ------------------------------------------------------------------------*/

#include <inttypes.h> /* commonly available, unlike <stdint.h> */

#if defined(__cplusplus)

# if !defined(HAVE_USHORT)
  typedef    unsigned short int    ushort;
# endif

# if !defined(HAVE_UINT)
  typedef    unsigned int          uint;
# endif

# if !defined(HAVE_ULONG)
  typedef    unsigned long int    ulong;
# endif

#endif


/* --------------------------------------------------------------------------
 * Macros for integer literal expressions
 * (Analagous to INTx_C() on Solaris/Linux, provided by inttypes.h)
 * ------------------------------------------------------------------------*/

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

#if (SIZEOF_VOIDP == 4)
# ifndef INT32_C /* use as a synecdoche for INT32_C and INT64_C */
#  define INT32_C(x)  x ## L
#  define UINT32_C(x) x ## UL
#  define INT64_C(x)  x ## LL
#  define UINT64_C(x) x ## ULL
# endif
#elif (SIZEOF_VOIDP == 8)
# ifndef INT32_C /* use as a synecdoche for xINT32_C and xINT64_C */
#  define INT32_C(x)  x
#  define UINT32_C(x) x
#  define INT64_C(x)  x ## L
#  define UINT64_C(x) x ## UL
# endif
#else
# error "Bad value for SIZEOF_VOIDP!"
#endif


/* --------------------------------------------------------------------------
 * printf format strings for constant sized types
 * (Analagous to PRIab() on Solaris/Linux, provided by inttypes.h)
 * ------------------------------------------------------------------------*/

#if (SIZEOF_VOIDP == 4)
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
#elif (SIZEOF_VOIDP == 8)
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
# error "Bad value for SIZEOF_VOIDP!"
#endif


/* --------------------------------------------------------------------------
 * MIN/MAX for C (use std::min/max for C++)
 * ------------------------------------------------------------------------*/

#if !defined(__cplusplus)

# undef MIN
# undef MAX
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
# define MAX(a,b) (((a) > (b)) ? (a) : (b))

#endif

/****************************************************************************/

#endif /* include_general_h */
