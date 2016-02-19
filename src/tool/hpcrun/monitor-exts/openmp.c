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

// Note: this must remain a separate file for proper linking with
// __wrap_ in the static case.

#include <messages/messages.h>
#include <monitor-exts/monitor_ext.h>

typedef void *mp_init_fcn(void);
static mp_init_fcn *real_mp_init = NULL;

#ifdef HPCRUN_STATIC_LINK
extern mp_init_fcn  __real__mp_init;
#endif

// The PGI OpenMP compiler does some strange things with their thread
// stacks.  We use _mp_init() as our test for this and then adjust the
// unwind heuristics if found.

void *
MONITOR_EXT_WRAP_NAME(_mp_init)(void)
{
  MONITOR_EXT_GET_NAME_WRAP(real_mp_init, _mp_init);

  ENABLE(OMP_SKIP_MSB);

  return (*real_mp_init)();
}
