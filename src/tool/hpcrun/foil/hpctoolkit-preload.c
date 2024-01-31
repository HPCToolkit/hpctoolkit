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
// Copyright ((c)) 2002-2024, Rice University
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

#include "foil.h"
#include "../start-stop.h"

#include "../hpctoolkit.h"

HPCRUN_EXPOSED int hpctoolkit_sampling_is_active() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_is_active);
  return base();
}

HPCRUN_EXPOSED void hpctoolkit_sampling_start() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_start);
  return base();
}

HPCRUN_EXPOSED void hpctoolkit_sampling_stop() {
  LOOKUP_FOIL_BASE(base, hpctoolkit_sampling_stop);
  return base();
}

// Fortran aliases

// FIXME: The Fortran functions really need a separate API with
// different names to handle arguments and return values.  But
// hpctoolkit_sampling_start() and _stop() are void->void, so they're
// a special case.

HPCRUN_EXPOSED void hpctoolkit_sampling_start_ () __attribute__ ((alias ("hpctoolkit_sampling_start")));
HPCRUN_EXPOSED void hpctoolkit_sampling_start__() __attribute__ ((alias ("hpctoolkit_sampling_start")));

HPCRUN_EXPOSED void hpctoolkit_sampling_stop_ () __attribute__ ((alias ("hpctoolkit_sampling_stop")));
HPCRUN_EXPOSED void hpctoolkit_sampling_stop__() __attribute__ ((alias ("hpctoolkit_sampling_stop")));
