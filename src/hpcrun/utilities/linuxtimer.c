// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "linuxtimer.h"



//*****************************************************************************
// local includes
//*****************************************************************************

// the man pages cite sigev_notify_thread_id in struct sigevent,
// but often the only name is a hidden union name.
#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif



//*****************************************************************************
// private operations
//*****************************************************************************

static void
timespec_set
(
 struct timespec *time,
 time_t sec,
 long nsec
)
{
  time->tv_sec = sec;
  time->tv_nsec = nsec;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

int
linuxtimer_create
(
 linuxtimer_t *td,
 clockid_t clock,
 int signal
)
{
  memset(&td->sigev, 0, sizeof(td->sigev));

  td->sigev.sigev_notify = SIGEV_THREAD_ID;
  td->sigev.sigev_signo = signal;
  td->sigev.sigev_value.sival_ptr = &td->timerid;
  td->sigev.sigev_notify_thread_id = syscall(SYS_gettid);

  int ret = timer_create(clock, &td->sigev, &td->timerid);

  if (ret) td->timerid = NULL;

  return ret;
}


int
linuxtimer_newsignal
(
 void
)
{
  // avoid signal conflict with papi (2), itimer (3), perf (4)
  return SIGRTMIN + 5;
}


int
linuxtimer_getsignal
(
 linuxtimer_t *td
)
{
  return td->sigev.sigev_signo;
}


int
linuxtimer_set
(
 linuxtimer_t *td,
 time_t sec,
 long nsec,
 bool repeat
)
{
  struct itimerspec when;

  timespec_set(&when.it_value, sec, nsec);
  timespec_set(&when.it_interval, repeat ? sec : 0, repeat ? nsec : 0);

  int no_flags = 0;
  int ret = timer_settime(td->timerid, no_flags, &when, 0);

  return ret;
}


int
linuxtimer_delete
(
 linuxtimer_t *td
)
{
  int ret = timer_delete(td->timerid);
  td->timerid = NULL;

  return ret;
}
