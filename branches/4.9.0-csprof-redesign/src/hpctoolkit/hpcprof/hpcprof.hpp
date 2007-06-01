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
#ifndef _hpcprof_h
#define _hpcprof_h

#include <include/general.h> /* special printf format strings */

//****************************************************************************
/* FIXME: This needs to be rewritten */

/* The version number. */
#define VPROF_VERSION "0.12"

/* pprof_off_t is an unsigned type with the same size as void*  */
/* typedef uintptr_t pprof_off_t; */

/* vmon_off64_t is an unsigned type with the size of 8. */
typedef uint64_t vmon_off64_t;

#endif /* _hpcprof_h */
