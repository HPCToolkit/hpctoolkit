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
// Copyright ((c)) 2002-2024, Rice University
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

//*****************************************************************************
// system includes
//*****************************************************************************

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
