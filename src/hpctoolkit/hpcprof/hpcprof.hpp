/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    Types for hpcprof
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (vconfig.h).
//
*****************************************************************************/


/* FIXME FIXME FIXME */

/* We want to replace this stuff.  I have set it up temporarily for
   IA32 and IA64 machines
*/

#ifndef _hpcprof_h
#define _hpcprof_h


/* --------------------------------------------------------- */
#if (__linux && __i386)
/* --------------------------------------------------------- */

/* The number of bytes in a unsigned long long.  If unsigned long long
   is not a valid type then, 0 */
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* The number of bytes in a unsigned long. */
#define SIZEOF_UNSIGNED_LONG 4

/* The number of bytes in a unsigned int. */
#define SIZEOF_UNSIGNED_INT 4

/* The number of bytes in a unsigned short. */
#define SIZEOF_UNSIGNED_SHORT 2

/* The number of bytes in a void *. */
#define SIZEOF_VOID_P 4

/* --------------------------------------------------------- */
#elif (__linux && (__ia64 || __x86_64))
/* --------------------------------------------------------- */

/* The number of bytes in a unsigned long long.  If unsigned long long
   is not a valid type then, 0 */
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* The number of bytes in a unsigned long. */
#define SIZEOF_UNSIGNED_LONG 8

/* The number of bytes in a unsigned int. */
#define SIZEOF_UNSIGNED_INT 4

/* The number of bytes in a unsigned short. */
#define SIZEOF_UNSIGNED_SHORT 2

/* The number of bytes in a void *. */
#define SIZEOF_VOID_P 8


#endif

//****************************************************************************

/* The version number. */
#define VPROF_VERSION "0.12"

/* Define if <cheader> style headers are present for C++ code.  */
/* #define HAVE_CNAME */

/* FIXME: pprof_off_t */
/* pprof_off_t is an unsigned type with the same size as void * */
#if SIZEOF_UNSIGNED_INT == SIZEOF_VOID_P
  typedef unsigned int pprof_off_t;
#elif SIZEOF_UNSIGNED_LONG == SIZEOF_VOID_P
  typedef unsigned long pprof_off_t;
#elif defined(SIZEOF_UNSIGNED_LONG_LONG) \
      && SIZEOF_UNSIGNED_LONG_LONG == SIZEOF_VOID_P
  typedef unsigned long long pprof_off_t;
#else
# error "could not find type for pprof_off_t"
#endif

/* vmon_off64_t is an unsigned type with the size of 8. */
#if SIZEOF_UNSIGNED_INT == 8
  typedef unsigned int vmon_off64_t;
#elif SIZEOF_UNSIGNED_LONG == 8
  typedef unsigned long vmon_off64_t;
#elif defined(SIZEOF_UNSIGNED_LONG_LONG) \
      && SIZEOF_UNSIGNED_LONG_LONG == 8
  typedef unsigned long long vmon_off64_t;
#else
# error "could not find type for vmon_off64_t"
#endif

#endif
