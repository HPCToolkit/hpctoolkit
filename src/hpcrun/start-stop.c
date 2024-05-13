// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
// (3) Sampling is initially on.  If you want it initially off, then
// set HPCRUN_DELAY_SAMPLING in the environment.
//
//***************************************************************************

#define _GNU_SOURCE

#include <stdlib.h>

#include "env.h"
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

  sampling_is_active = ! hpcrun_get_env_bool("HPCRUN_DELAY_SAMPLING");
  dont_reinit = 1;
}


int
foilbase_hpctoolkit_sampling_is_active(void)
{
  return sampling_is_active;
}


void
foilbase_hpctoolkit_sampling_start(void)
{
  sampling_is_active = 1;
  dont_reinit = 1;
  if (! SAMPLE_SOURCES(started)) {
    SAMPLE_SOURCES(start);
  }
}


void
foilbase_hpctoolkit_sampling_stop(void)
{
  sampling_is_active = 0;
  dont_reinit = 1;
  if (SAMPLE_SOURCES(started)) {
    SAMPLE_SOURCES(stop);
  }
}
