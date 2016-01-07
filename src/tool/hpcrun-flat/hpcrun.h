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

/****************************************************************************
//
// File:
//    $HeadURL$
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

#define HPCRUN_LIB  "libhpcrun-flat.so"

typedef enum enum_hpc_threadprof_t {
  HPCRUN_THREADPROF_EACH = 0, /* separate profile for each thread */
#define HPCRUN_THREADPROF_EACH_STR "0"
  
  HPCRUN_THREADPROF_ALL  = 1  /* combined profile for all thread */
#define HPCRUN_THREADPROF_ALL_STR  "1"
} hpc_threadprof_t;


/* Special system supported events */

#define HPCRUN_EVENT_WALLCLK_STR     "WALLCLOCK"
#define HPCRUN_EVENT_WALLCLK_STRLN   9

#define HPCRUN_EVENT_FWALLCLK_STR    "FWALLCLOCK"
#define HPCRUN_EVENT_FWALLCLK_STRLN  10

/****************************************************************************/

#endif
