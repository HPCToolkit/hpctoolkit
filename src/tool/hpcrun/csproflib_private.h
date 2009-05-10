// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    csproflib_private.h
//
// Purpose:
//    Private functions, structures, etc. used in the profiler (in
//    contrast to the profiler's public interface).
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSPROFLIB_PRIVATE_H
#define CSPROFLIB_PRIVATE_H

//************************* System Include Files ****************************

#include <limits.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#ifdef __osf__
#include <excpt.h>              /* Alpha-specific */
#endif

//*************************** User Include Files ****************************

#include "list.h"
#include "epoch.h"

#include "structs.h"

//*************************** Forward Declarations **************************

#include "fname_max.h"

typedef void (*sig_handler_func_t)(int, siginfo_t *, void *);

//***************************************************************************

//***************************************************************************
// helper structures for csprof_state_t: 
//***************************************************************************

// Defaults (see general.h for verbosity and debug)
#define CSPROF_BACKTRACE_CACHE_INIT_SZ 32


//***************************************************************************
// csprof_pfmon_info
//***************************************************************************

// Abbrev. Quick Reference (See "Intel Itanium Architecture Software
// Developer's Manual, Volume 2: System Architecture" for detailed
// explanations.)
//   PMU: performance monitoring unit
//   PMC: performance monitor configuration register
//   PMD: performance monitor data register
//   PSR: processor status register

#if 0
#define CSPROF_PFMON_N_EVENTS     2
#define CSPROF_PFMON_N_PMCS       PMU_MAX_PMCS
#define CSPROF_PFMON_N_PMDS       PMU_MAX_PMDS

// List of events to monitor
char *csprof_pfmon_evt_list[CSPROF_PFMON_N_EVENTS] = {
  "cpu_cycles",
  "IA64_INST_RETIRED"
};

// ---------------------------------------------------------
// csprof_pfmon_info_t:
// ---------------------------------------------------------
typedef struct csprof_pfmon_info_s {

  // pfmon data structures 
  pfmlib_param_t evt;       // basic params: events, etc.
  pfmlib_options_t opt;     // options
  pfarg_context_t ctx[1];   // monitoring context
  pfarg_reg_t pmc[CSPROF_PFMON_N_PMCS]; // to write/read PMC registers
  pfarg_reg_t pmd[CSPROF_PFMON_N_PMDS]; // to write/read PMD registers
  struct sigaction sigact;  // signal handler

} csprof_pfmon_info_t;

static int csprof_pfmon_info__init(csprof_pfmon_info_t* pfm);
static int csprof_pfmon_info__fini(csprof_pfmon_info_t* pfm);

//***************************************************************************
#endif /* 0 */

/* FIXME: make the explicit interface return a value */

#if 0
void csprof_init_internal(void);
void csprof_fini_internal(csprof_state_t *state);
#endif

#include "csprof_monitor_callbacks.h"

#ifdef CSPROF_TRAMPOLINE_BACKEND
// capturing function call returns
static void csprof_undo_swizzled_data(csprof_state_t *, void *);

void *csprof_trampoline2(void **);
#endif

int csprof_write_profile_data(csprof_state_t *);

#ifdef CSPROF_THREADS
void csprof_atexit_handler();
#endif

#endif
