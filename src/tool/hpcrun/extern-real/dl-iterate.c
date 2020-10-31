// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2020, Rice University
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

// Override dl_iterate_phdr() from libunwind and supply an answer from
// hpcrun without calling the real version.  dl_iterate_phdr acquires
// a lock which may deadlock when called from a signal handler.
//
// Note: we only track dlopen/dlclose in the dynamic case.  Libunwind
// doesn't distinguish between static and dynamic cases and needs a
// real answer (more than just one module) even in the static case,
// or else it gives 100% bad unwinds (on arm).
//
// Currently, we fallback to calling the real version in the static
// case.  But a static program probably doesn't call dlopen/dlclose,
// so there is not much chance of deadlock.

//----------------------------------------------------------------------

#define _GNU_SOURCE  1

#include <sys/types.h>
#include <link.h>
#include "loadmap.h"

int
hpcrun_real_dl_iterate_phdr (
  int (*callback) (struct dl_phdr_info * info, size_t size, void * data),
  void *data
)
{
#ifdef HPCRUN_STATIC_LINK
  // statically, we have no replacement, so call the real version
  return dl_iterate_phdr(callback, data);

#else
  // dynamically, call our replacement version
  return hpcrun_loadmap_iterate(callback, data);

#endif
}
