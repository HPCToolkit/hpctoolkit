/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    General header.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef hpcrun_h
#define hpcrun_h

/************************** System Include Files ****************************/

#include <stdio.h>
#include <lib/prof-lean/hpcrunflat-fmt.h>

/**************************** Forward Declarations **************************/

/* 
   Options to the hpcrun preloaded library (passed via the environment):
   
   Variable              Value
   ---------------------------------------------------------------------
   HPCRUN_RECURSIVE      0 for no; 1 for yes
   HPCRUN_THREAD         see hpc_threadprof_t below
   HPCRUN_EVENT_LIST     <event1>:<period1>;<event2>;<event3>:<period3>...
                         (period is optional)
   HPCRUN_OUTPUT_PATH    output file path
   HPCRUN_EVENT_FLAG     a papi sprofil() flag
   HPCRUN_DEBUG          positive integer
 */


// tallent: temporarily create this
#define HPCRUN_SICORTEX 0
#if (HPCRUN_SICORTEX)
# define HPCRUN_NAME "hpcex"
#else
# define HPCRUN_NAME "hpcrun-flat"
#endif

#define HPCRUN_LIB  "libhpcrun.so"

typedef enum enum_hpc_threadprof_t {
  HPCRUN_THREADPROF_EACH = 0, /* separate profile for each thread */
#define HPCRUN_THREADPROF_EACH_STR "0"
  
  HPCRUN_THREADPROF_ALL  = 1  /* combined profile for all thread */
#define HPCRUN_THREADPROF_ALL_STR  "1"
} hpc_threadprof_t;


/* Special system supported events */

#define HPCRUN_EVENT_WALLCLK_STR     "WALLCLK"
#define HPCRUN_EVENT_WALLCLK_STRLN   7

#define HPCRUN_EVENT_FWALLCLK_STR    "FWALLCLK"
#define HPCRUN_EVENT_FWALLCLK_STRLN  8

/****************************************************************************/

#endif
