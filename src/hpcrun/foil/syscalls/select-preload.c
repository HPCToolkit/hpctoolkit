// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//------------------------------------------------------------------------------
// File: select.c
//
// Purpose:
//   wrapper for libc select. if select returns a -1 because it was
//   interrupted, assume the interrupt was from asynchronous sampling
//   caused by hpcrun and restart.
//
// Note:
//   The kernel automatically updates the timeout parameter for select(),
//   but not for poll(), ppoll() or pselect().
//------------------------------------------------------------------------------

#define _GNU_SOURCE

#include "../foil.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


//******************************************************************************
// interface operations
//******************************************************************************

HPCRUN_EXPOSED int select
(
  int nfds,
  fd_set *read_fds,
  fd_set *write_fds,
  fd_set *except_fds,
  struct timeval *timeout
)
{
  FOIL_DLSYM(real_select, select);

  int retval;
  int incoming_errno = errno; // save incoming errno

  for(;;) {
    retval = real_select(nfds, read_fds, write_fds, except_fds, timeout);

    if (retval == -1) {
      if (errno == EINTR) {
        // restart on EINTR
        errno = incoming_errno; // restore incoming errno
        continue;
      }
    }

    // return otherwise
    break;
  }

  return retval;
}
