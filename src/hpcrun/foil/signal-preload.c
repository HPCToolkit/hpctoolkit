// SPDX-FileCopyrightText: 2002-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
