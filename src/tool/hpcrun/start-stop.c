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

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// This file provides an API for the application to start and stop
// sampling with explicit function calls in the application source.
//
// Note:
// (1) The actions are process-wide.
//
// (2) The start and stop functions turn the sample sources on and off
// in the current thread.  The sources in the other threads continue
// to run but don't unwind and don't record samples.  Stopping
// sampling in all threads is more complicated because it would have
// to be done in a signal handler which can't really be done safely.
//
// (3) Sampling is initally on.  If you want it initially off, then
// set HPCRUN_DELAY_SAMPLING in the environment.
//
//***************************************************************************

#include <stdlib.h>

#include "sample_sources_all.h"
#include "start-stop.h"

static int sampling_is_active = 1;
static int dont_reinit = 0;


//***************************************************************************
// interface functions
//***************************************************************************

void
hpcrun_start_stop_internal_init(void)
{
  // Make sure we don't run init twice.  This is for the case that the
  // application turns sampling on/off and then forks.  We want to
  // preserve the state across fork and not reset it in the child.
  if (dont_reinit) {
    return;
  }
  if (getenv("HPCRUN_DELAY_SAMPLING") != NULL) {
    sampling_is_active = 0;
  }
  dont_reinit = 1;
}


int
hpctoolkit_sampling_is_active(void)
{
  return sampling_is_active;
}


void
hpctoolkit_sampling_start(void)
{
  sampling_is_active = 1;
  dont_reinit = 1;
  if (! SAMPLE_SOURCES(started)) {
    SAMPLE_SOURCES(start);
  }
}


void
hpctoolkit_sampling_stop(void)
{
  sampling_is_active = 0;
  dont_reinit = 1;
  if (SAMPLE_SOURCES(started)) {
    SAMPLE_SOURCES(stop);
  }
}


// Fortran aliases

// FIXME: The Fortran functions really need a separate API with
// different names to handle arguments and return values.  But
// hpctoolkit_sampling_start() and _stop() are void->void, so they're
// a special case.

void hpctoolkit_sampling_start_ (void) __attribute__ ((alias ("hpctoolkit_sampling_start")));
void hpctoolkit_sampling_start__(void) __attribute__ ((alias ("hpctoolkit_sampling_start")));

void hpctoolkit_sampling_stop_ (void) __attribute__ ((alias ("hpctoolkit_sampling_stop")));
void hpctoolkit_sampling_stop__(void) __attribute__ ((alias ("hpctoolkit_sampling_stop")));
