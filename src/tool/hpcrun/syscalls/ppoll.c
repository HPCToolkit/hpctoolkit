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

//
//  Wrappers for ppoll and pselect.  If interrupted by a signal with
//  EINTR, then automatically restart.
//
//  Note: the timespec timeout can be positive (wait for timeout),
//  zero (return immediately), negative or NULL (wait forever).  We
//  only update the timeout if it exists (non-NULL) and is positive,
//  otherwise, just pass on the original value.
//

//******************************************************************************
// system includes, typedefs
//******************************************************************************

#define _GNU_SOURCE  1

#include <sys/types.h>
#include <sys/select.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif

#include <monitor-exts/monitor_ext.h>

#define BILLION  1000000000

typedef int ppoll_fn (struct pollfd *, nfds_t,
		      const struct timespec *, const sigset_t *);

typedef int pselect_fn (int, fd_set *, fd_set *, fd_set *,
			const struct timespec *, const sigset_t *);

#ifdef HPCRUN_STATIC_LINK
extern ppoll_fn    __real_ppoll;
extern pselect_fn  __real_pselect;
#endif

static ppoll_fn   *real_ppoll = NULL;
static pselect_fn *real_pselect = NULL;


//******************************************************************************
// local operations
//******************************************************************************

static void
find_ppoll(void)
{
#ifdef HPCRUN_STATIC_LINK
  real_ppoll = __real_ppoll;
#else
  real_ppoll = (ppoll_fn *) dlsym(RTLD_NEXT, "ppoll");
#endif

  assert(real_ppoll);
}

static void
find_pselect(void)
{
#ifdef HPCRUN_STATIC_LINK
  real_pselect = __real_pselect;
#else
  real_pselect = (pselect_fn *) dlsym(RTLD_NEXT, "pselect");
#endif

  assert(real_pselect);
}

//----------------------------------------------------------------------
//
// Both tspec_add() and tspec_sub() assume that the inputs are
// normalized, that is, 0 <= nsec < BILLION.
//
// Returns: ans <-- a + b
//
static inline void
tspec_add(struct timespec *ans, const struct timespec *a, const struct timespec *b)
{
  ans->tv_sec  = a->tv_sec + b->tv_sec;
  ans->tv_nsec = a->tv_nsec + b->tv_nsec;

  if (ans->tv_nsec >= BILLION) {
    ans->tv_sec++;
    ans->tv_nsec -= BILLION;
  }
}

//
// Returns: ans <-- a - b
//
static inline void
tspec_sub(struct timespec *ans, const struct timespec *a, const struct timespec *b)
{
  ans->tv_sec  = a->tv_sec - b->tv_sec;
  ans->tv_nsec = a->tv_nsec - b->tv_nsec;

  if (ans->tv_nsec < 0) {
    ans->tv_sec--;
    ans->tv_nsec += BILLION;
  }
}


//******************************************************************************
// interface operations
//******************************************************************************

int
MONITOR_EXT_WRAP_NAME(ppoll)
  (struct pollfd *fds, nfds_t nfds,
   const struct timespec *init_timeout, const sigset_t *sigmask)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_ppoll);

  struct timespec end, now, my_timeout, *timeout_ptr;
  int init_errno = errno;
  int ret;

  //
  // update timeout only if non-NULL and > 0, otherwise just pass on
  // the original value.
  //
  int update_timeout = (init_timeout != NULL) &&
      (init_timeout->tv_sec > 0 || (init_timeout->tv_sec == 0 && init_timeout->tv_nsec > 0));

  if (update_timeout) {
    clock_gettime(CLOCK_REALTIME, &now);
    tspec_add(&end, &now, init_timeout);
    my_timeout = *init_timeout;
    timeout_ptr = &my_timeout;
  }
  else {
    timeout_ptr = (struct timespec *) init_timeout;
  }

  for (;;) {
    ret = (* real_ppoll) (fds, nfds, timeout_ptr, sigmask);

    if (! (ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = init_errno;

    // adjust timout and restart syscall
    if (update_timeout) {
      clock_gettime(CLOCK_REALTIME, &now);
      tspec_sub(&my_timeout, &end, &now);

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (my_timeout.tv_sec < 0 || my_timeout.tv_nsec < 0) {
	my_timeout.tv_sec = 0;
	my_timeout.tv_nsec = 0;
      }
    }
  }

  return ret;
}

//----------------------------------------------------------------------

int
MONITOR_EXT_WRAP_NAME(pselect)
  (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
   const struct timespec *init_timeout, const sigset_t *sigmask)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_pselect);

  struct timespec end, now, my_timeout, *timeout_ptr;
  int init_errno = errno;
  int ret;

  //
  // update timeout only if non-NULL and > 0, otherwise just pass on
  // the original value.
  //
  int update_timeout = (init_timeout != NULL) &&
      (init_timeout->tv_sec > 0 || (init_timeout->tv_sec == 0 && init_timeout->tv_nsec > 0));

  if (update_timeout) {
    clock_gettime(CLOCK_REALTIME, &now);
    tspec_add(&end, &now, init_timeout);
    my_timeout = *init_timeout;
    timeout_ptr = &my_timeout;
  }
  else {
    timeout_ptr = (struct timespec *) init_timeout;
  }

  for (;;) {
    ret = (* real_pselect) (nfds, readfds, writefds, exceptfds, timeout_ptr, sigmask);

    if (! (ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = init_errno;

    // adjust timout and restart syscall
    if (update_timeout) {
      clock_gettime(CLOCK_REALTIME, &now);
      tspec_sub(&my_timeout, &end, &now);

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (my_timeout.tv_sec < 0 || my_timeout.tv_nsec < 0) {
	my_timeout.tv_sec = 0;
	my_timeout.tv_nsec = 0;
      }
    }
  }

  return ret;
}
