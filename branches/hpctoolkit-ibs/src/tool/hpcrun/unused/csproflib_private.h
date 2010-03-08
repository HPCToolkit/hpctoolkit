// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include "epoch.h"
#include "state.h"

//*************************** Forward Declarations **************************

#include "fname_max.h"

typedef void (*sig_handler_func_t)(int, siginfo_t *, void *);

//***************************************************************************

//***************************************************************************
// hpcrun_pfmon_info
//***************************************************************************

// Abbrev. Quick Reference (See "Intel Itanium Architecture Software
// Developer's Manual, Volume 2: System Architecture" for detailed
// explanations.)
//   PMU: performance monitoring unit
//   PMC: performance monitor configuration register
//   PMD: performance monitor data register
//   PSR: processor status register

int hpcrun_write_profile_data(state_t*);

void hpcrun_atexit_handler();

#endif
