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
// Copyright ((c)) 2002-2023, Rice University
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

#define _GNU_SOURCE

#include "foil.h"
#include "../libmonitor/monitor.h"
#include <signal.h>

HPCRUN_EXPOSED int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
  LOOKUP_FOIL_BASE(base, sigaction);
  return base(sig, act, oldact);
}

HPCRUN_EXPOSED sighandler_t signal(int sig, sighandler_t handler) {
  LOOKUP_FOIL_BASE(base, signal);
  return base(sig, handler);
}

HPCRUN_EXPOSED int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  LOOKUP_FOIL_BASE(base, sigprocmask);
  return base(how, set, oldset);
}

HPCRUN_EXPOSED int sigwait(const sigset_t* set, int* sig) {
  LOOKUP_FOIL_BASE(base, sigwait);
  return base(set, sig);
}

HPCRUN_EXPOSED int sigwaitinfo(const sigset_t* set, siginfo_t* info) {
  LOOKUP_FOIL_BASE(base, sigwaitinfo);
  return base(set, info);
}

HPCRUN_EXPOSED int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
  LOOKUP_FOIL_BASE(base, sigtimedwait);
  return base(set, info, timeout);
}
