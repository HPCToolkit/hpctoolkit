// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// Provides three functions:
//
//   hpcrun_block_profile_signal(sigset_t * oldset)
//   hpcrun_block_shootdown_signal(sigset_t * oldset)
//   hpcrun_restore_sigmask(sigset_t * oldset)
//
// We don't want an unwind sample to be interrupted by the shootdown
// signal and we don't want the fini-thread callback (from shootdown
// handler) to be interrupted by a sample interrupt.  These functions
// can be used in the interrupt handler or fini-thread callback to
// block the unwanted signals.
//
// oldset is returned as the original signal mask to restore via
//   hpcrun_restore_sigmask()
//
// Note: if you block a signal in one scope, don't siglongjmp()
// outside of that scope without restoring the signal mask.
//
// We could set the signal mask via monitor_sigaction(), except that
// doesn't work for PAPI because we don't control the call to
// sigaction.  But setting it in the handler is just as good.

//----------------------------------------------------------------------

#define _GNU_SOURCE

#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include "libmonitor/monitor.h"
#include "hpcrun_signals.h"
#include "audit/audit-api.h"

// Maximum number of signals
#ifndef NSIG
#define NSIG  128
#endif

#define SHOOTDOWN_VAR  "MONITOR_SHOOTDOWN_SIGNAL"

static pthread_once_t signal_once = PTHREAD_ONCE_INIT;

static sigset_t the_profile_mask;
static sigset_t the_shootdown_mask;

//----------------------------------------------------------------------

// Set the profile and shootdown signal masks for all threads to use.
//
// The POSIX and Perf signals are internal to hpcrun.  PAPI
// theoretically could choose any signal, but in practice always uses
// SIGRTMIN+2.  We could add an environ variable for PAPI, but they
// don't expose the signal in a structured way.
//
// Libmonitor, in practice, always uses SIGRTMIN+8 but has an environ
// variable in case that conflicts with something.
//
static void
hpcrun_signals_init_internal(void)
{
  sigemptyset(&the_profile_mask);
  sigaddset(&the_profile_mask, SIGRTMIN + 2);  // papi
  sigaddset(&the_profile_mask, SIGRTMIN + 3);  // posix
  sigaddset(&the_profile_mask, SIGRTMIN + 4);  // perf

  int shootdown_signal = SIGRTMIN + 8;  // libmonitor
  char * str = getenv(SHOOTDOWN_VAR);

  if (str != NULL) {
    int val = atoi(str);
    if (0 < val && val < NSIG) {
      shootdown_signal = val;
    }
  }

  sigemptyset(&the_shootdown_mask);
  sigaddset(&the_shootdown_mask, shootdown_signal);
}

//
// Be extra careful that we are properly initialized.
//
void
hpcrun_signals_init(void)
{
  pthread_once(&signal_once, hpcrun_signals_init_internal);
}

//
// Returns: result from pthread_sigmask()
//
// oldset is returned as the original signal mask
//
int
hpcrun_block_profile_signal(sigset_t * oldset)
{
  hpcrun_signals_init();

  return auditor_exports->pthread_sigmask(SIG_BLOCK, &the_profile_mask, oldset);
}

int
hpcrun_block_shootdown_signal(sigset_t * oldset)
{
  hpcrun_signals_init();

  return auditor_exports->pthread_sigmask(SIG_BLOCK, &the_shootdown_mask, oldset);
}

int
hpcrun_restore_sigmask(sigset_t * oldset)
{
  return auditor_exports->pthread_sigmask(SIG_SETMASK, oldset, NULL);
}

//----------------------------------------------------------------------

void
hpcrun_drain_signal(int sig)
{
  sigset_t set;
  struct timespec timeout = {0, 0};

  sigemptyset(&set);
  sigaddset(&set, sig);

  // wait until no signals returned
  for (;;) {
    if (sigtimedwait(&set, NULL, &timeout) < 0) {
      break;
    }
  }
}
