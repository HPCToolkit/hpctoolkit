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
// This file contains default no-op definitions for the sampling
// start/stop functions so that an application can link with
// -lhpctoolkit without having to #ifdef all of the calls.
//
//***************************************************************************

#define _GNU_SOURCE

#include "hpctoolkit.h"

void __attribute__ ((weak))
hpctoolkit_sampling_start(void)
{
}

void __attribute__ ((weak))
hpctoolkit_sampling_stop(void)
{
}

int __attribute__ ((weak))
hpctoolkit_sampling_is_active(void)
{
  return 0;
}

// Fortran aliases

// FIXME: The Fortran functions really need a separate API with
// different names to handle arguments and return values.  But
// hpctoolkit_sampling_start() and _stop() are void->void, so they're
// a special case.

void hpctoolkit_sampling_start_ (void)
__attribute__((weak, alias("hpctoolkit_sampling_start"), visibility("default")));
void hpctoolkit_sampling_start__(void)
__attribute__((weak, alias("hpctoolkit_sampling_start"), visibility("default")));

void hpctoolkit_sampling_stop_ (void)
__attribute__((weak, alias("hpctoolkit_sampling_stop"), visibility("default")));
void hpctoolkit_sampling_stop__(void)
__attribute__((weak, alias("hpctoolkit_sampling_stop"), visibility("default")));
