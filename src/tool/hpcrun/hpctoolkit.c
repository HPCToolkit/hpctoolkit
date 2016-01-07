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
// This file contains default no-op definitions for the sampling
// start/stop functions so that an application can link with
// -lhpctoolkit without having to #ifdef all of the calls.
//
// The static case must use weak symbols, but the dynamic case can use
// strong or weak (LD_PRELOAD supersedes strong/weak).
//
//***************************************************************************

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

void hpctoolkit_sampling_start_ (void) __attribute__ ((weak, alias ("hpctoolkit_sampling_start")));
void hpctoolkit_sampling_start__(void) __attribute__ ((weak, alias ("hpctoolkit_sampling_start")));

void hpctoolkit_sampling_stop_ (void) __attribute__ ((weak, alias ("hpctoolkit_sampling_stop")));
void hpctoolkit_sampling_stop__(void) __attribute__ ((weak, alias ("hpctoolkit_sampling_stop")));
