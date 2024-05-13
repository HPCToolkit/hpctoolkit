// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//------------------------------------------------------------------------------
// File: poll.c
//
// Purpose:
//   wrapper for libc poll. if poll returns a -1 because it was interrupted,
//   assume the interrupt was from asynchronous sampling caused by hpcrun and
//   restart.
//------------------------------------------------------------------------------

#define _GNU_SOURCE

#include "../foil.h"

#include <sys/types.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>


#define MILLION     1000000
#define THOUSAND       1000

//******************************************************************************
// interface operations
//******************************************************************************

HPCRUN_EXPOSED int poll(struct pollfd *fds, nfds_t nfds, int init_timeout) {
  FOIL_DLSYM(real_poll, poll);

  struct timespec start, now;
  int incoming_errno = errno; // save incoming errno
  int ret;

  if (init_timeout > 0) {
    clock_gettime(CLOCK_REALTIME, &start);
  }

  int timeout = init_timeout;

  for(;;) {
    // call the libc poll operation
    ret = real_poll(fds, nfds, timeout);

    if (! (ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = incoming_errno;

    // adjust timeout and restart syscall
    if (init_timeout > 0) {
      clock_gettime(CLOCK_REALTIME, &now);

      timeout = init_timeout - THOUSAND * (now.tv_sec - start.tv_sec)
          - (now.tv_nsec - start.tv_nsec) / MILLION;

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (timeout < 0) {
        timeout = 0;
      }
    }
  }

  return ret;
}
