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

//------------------------------------------------------------------------------
// File: poll.c 
//  
// Purpose: 
//   wrapper for libc poll. if poll returns a -1 because it was interrupted, 
//   assume the interrupt was from asynchronous sampling caused by hpcrun and 
//   restart. 
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE  1

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif

#include <monitor-exts/monitor_ext.h>

#define MILLION     1000000
#define THOUSAND       1000


//******************************************************************************
// type declarations
//******************************************************************************

typedef int poll_fn (struct pollfd *fds, nfds_t nfds, int timeout);


//******************************************************************************
// local data
//******************************************************************************

#ifdef HPCRUN_STATIC_LINK
extern poll_fn  __real_poll;
#endif

static poll_fn *real_poll = NULL;


//******************************************************************************
// local operations
//******************************************************************************

static void 
find_poll(void)
{
#ifdef HPCRUN_STATIC_LINK
  real_poll = __real_poll;
#else
  real_poll = (poll_fn *) dlsym(RTLD_NEXT, "poll");
#endif

  assert(real_poll);
}


//******************************************************************************
// interface operations
//******************************************************************************

int 
MONITOR_EXT_WRAP_NAME(poll)
  (struct pollfd *fds, nfds_t nfds, int init_timeout)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_poll);

  struct timespec start, now;
  int incoming_errno = errno; // save incoming errno
  int ret;

  if (init_timeout > 0) {
    clock_gettime(CLOCK_REALTIME, &start);
  }

  int timeout = init_timeout;

  for(;;) {
    // call the libc poll operation
    ret = (* real_poll) (fds, nfds, timeout);

    if (! (ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = incoming_errno;

    // adjust timout and restart syscall
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
